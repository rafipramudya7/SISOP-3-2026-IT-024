

#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>


typedef struct {
    int    fd;                
    char   name[1000];  
    int    cekAdmin;        
    int    cekslot;          
} dataclient;

static dataclient  clients[64];   
static int client_count = 0;      
static pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;
static int server_fd = -1;      
static time_t server_start;    
static int shutdown_pipe[2];


// fungsi buat nulis log
static void write_log(const char *actor, const char *status) {
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    pthread_mutex_lock(&logMutex);       
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "%s [%s] [%s]\n", ts, actor, status);
        fclose(f);
    }
    pthread_mutex_unlock(&logMutex);     
}


// fungsi untik meneruskan pesan
static void broadcast(Packet *pkt, int ownFd) {
    pthread_mutex_lock(&clientMutex);
    for (int i = 0; i < 64; i++) {
        if (clients[i].cekslot && clients[i].fd != ownFd) {
            send(clients[i].fd, pkt, sizeof(Packet), 0);
        }
    }
    pthread_mutex_unlock(&clientMutex);
}

static int find_slot(void) {
    for (int i = 0; i < 64; i++) {
        if (!clients[i].cekslot) return i;
    }
    return -1;
}

static int name_exists(const char *name) {
    for (int i = 0; i < 64; i++) {
        if (clients[i].cekslot && strcmp(clients[i].name, name) == 0) return 1;
    }
    return 0;
}
static void rpcAdmin(int slot, RpcCommand cmd) {
    Packet res;
    memset(&res, 0, sizeof(res));
    res.type = MSG_RPC_RES;
    strcpy(res.sender, "System");

    char log_entry[1000];

    if (cmd == RPC_GET_USERS) {
        pthread_mutex_lock(&clientMutex);
        int count = 0;
        char list[1000] = "";
        for (int i = 0; i < 64; i++) {
            if (clients[i].cekslot && !clients[i].cekAdmin) {
                if (count > 0) strcat(list, ", ");
                strcat(list, clients[i].name);
                count++;
            }
        }
        pthread_mutex_unlock(&clientMutex);

        snprintf(res.body, 1000,
                 "cekslot entities: %d | %s", count, list);
        send(clients[slot].fd, &res, sizeof(res), 0);

        snprintf(log_entry, 1000, "RPC_GET_USERS");
        write_log("Admin", log_entry);
    } else if (cmd == RPC_GET_UPTIME) {
        time_t now = time(NULL);
        long total = (long)(now - server_start);
        long jam = total / 3600;
        long menit = (total % 3600) / 60;
        long detik = total % 60;
        snprintf(res.body, 1000,
                 "Server uptime: %02ldh %02ldm %02lds", jam, menit, detik);
        send(clients[slot].fd, &res, sizeof(res), 0);
        write_log("Admin", "RPC_GET_UPTIME");
    } else if (cmd == RPC_SHUTDOWN) {
        snprintf(res.body, 1000, "EMERGENCY SHUTDOWN ");
        send(clients[slot].fd, &res, sizeof(res), 0);
        write_log("Admin", "RPC_SHUTDOWN");
        write_log("System", "EMERGENCY SHUTDOWN ");

        char sig = 1;
        write(shutdown_pipe[1], &sig, 1);
    }
}

static void *handle_client_thread(void *arg) {
    int slot = *(int *)arg;
    free(arg);
    int fd = clients[slot].fd;
    Packet pkt;
    char ts[32], log_entry[3000];

    // hanle register
    while (1) {
        if (recv(fd, &pkt, sizeof(pkt), 0) <= 0) goto cleanup;

        pthread_mutex_lock(&clientMutex);
        int dup = name_exists(pkt.body);
        pthread_mutex_unlock(&clientMutex);

        if (dup) {
            /* Kirim penolakan → client harus coba nama lain */
            Packet rej;
            memset(&rej, 0, sizeof(rej));
            rej.type = MSG_SYSTEM;
            snprintf(rej.body, 1000,
                "[System] The identity '%s' is already synchronized in The Wired.",
                pkt.body);
            send(fd, &rej, sizeof(rej), 0);
        } else {
            /* Nama diterima → simpan ke slot */
            pthread_mutex_lock(&clientMutex);
            strncpy(clients[slot].name, pkt.body, 1000);
            client_count++;
            pthread_mutex_unlock(&clientMutex);
            break;
        }
    }
    // CEK ADMIN
    pthread_mutex_lock(&clientMutex);
    int is_knights = (strcmp(clients[slot].name, "The Knights") == 0);
    pthread_mutex_unlock(&clientMutex);
    if (is_knights) {
        /* Minta password */
        Packet ask;
        memset(&ask, 0, sizeof(ask));
        ask.type = MSG_AUTH;
        strcpy(ask.body, "PASSWORD_REQUIRED");
        send(fd, &ask, sizeof(ask), 0);
        if (recv(fd, &pkt, sizeof(pkt), 0) <= 0) goto cleanup;
        if (strcmp(pkt.body, ADMIN_PASSWORD) == 0) {
            pthread_mutex_lock(&clientMutex);
            clients[slot].cekAdmin = 1;
            pthread_mutex_unlock(&clientMutex);
            Packet ok;
            memset(&ok, 0, sizeof(ok));
            ok.type = MSG_AUTH;
            strcpy(ok.body, "AUTH_OK");
            send(fd, &ok, sizeof(ok), 0);
            write_log("System", "User 'The Knights' connected");
        } else {
            Packet fail;
            memset(&fail, 0, sizeof(fail));
            fail.type = MSG_AUTH;
            strcpy(fail.body, "AUTH_FAIL");
            send(fd, &fail, sizeof(fail), 0);
            goto cleanup;
        }
    } else {
        // kirim pesan selamat
        Packet welcome;
        memset(&welcome, 0, sizeof(welcome));
        welcome.type = MSG_SYSTEM;
        snprintf(welcome.body, 1000,
                 "--- Welcome to The Wired, %s ---", clients[slot].name);
        send(fd, &welcome, sizeof(welcome), 0);

        Packet notif;
        memset(&notif, 0, sizeof(notif));
        notif.type = MSG_SYSTEM;
        snprintf(notif.body, 1000,
                 "[System] User '%s' connected", clients[slot].name);
        broadcast(&notif, fd);
        snprintf(log_entry, 1000,
                 "User '%s' connected", clients[slot].name);
        write_log("System", log_entry);
    }

    while (1) {
        ssize_t n = recv(fd, &pkt, sizeof(pkt), 0);
        if (n <= 0) break;  

        pthread_mutex_lock(&clientMutex);
        char sender_name[1000];
        int  sender_admin = clients[slot].cekAdmin;
        strncpy(sender_name, clients[slot].name, 1000);
        pthread_mutex_unlock(&clientMutex);

        if (pkt.type == MSG_EXIT) {
            break;

        } else if (pkt.type == MSG_CHAT) {

            char display[3000];
            snprintf(display, sizeof(display),
                     "[%s]: %s", sender_name, pkt.body);

            Packet out;
            memset(&out, 0, sizeof(out));
            out.type = MSG_CHAT;
            strncpy(out.sender, sender_name, 999);
            strncpy(out.body, display, 999);
            broadcast(&out, fd);

            snprintf(log_entry, sizeof(log_entry),
                     "[%s]: %s", sender_name, pkt.body);
            write_log("User", log_entry);

        } else if (pkt.type == MSG_RPC_REQ && sender_admin) {
            rpcAdmin(slot, (RpcCommand)pkt.rpc_cmd);
        }
    }

cleanup:
    pthread_mutex_lock(&clientMutex);
    char name_copy[1000];
    strncpy(name_copy, clients[slot].name, 999);
    clients[slot].cekslot = 0;
    clients[slot].fd     = -1;
    clients[slot].name[0]= '\0';
    clients[slot].cekAdmin = 0;
    if (client_count > 0) client_count--;
    pthread_mutex_unlock(&clientMutex);

    Packet bye;
    memset(&bye, 0, sizeof(bye));
    bye.type = MSG_SYSTEM;
    snprintf(bye.body, 1000, "[System] User '%s' disconnected", name_copy);
    broadcast(&bye, -1);

    snprintf(log_entry, 1000, "User '%s' disconnected", name_copy);
    write_log("System", log_entry);
    Packet dc;
    memset(&dc, 0, sizeof(dc));
    dc.type = MSG_SYSTEM;
    strcpy(dc.body, "[System] Disconnecting from The Wired...");
    send(fd, &dc, sizeof(dc), 0);

    close(fd);
    pthread_detach(pthread_self());
    return NULL;
}


static void sigint_handler(int sig) {
    (void)sig;
    write_log("System", "SERVER OFFLINE");
    printf("\n[System] Shutting down The Wired...\n");
    if (server_fd >= 0) close(server_fd);
    exit(0);
}



int main(void) {
    if (pipe(shutdown_pipe) < 0) {
        perror("pipe");
        return 1;
    }

    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);   

    server_start = time(NULL);
    memset(clients, 0, sizeof(clients));
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(SERVER_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen"); return 1;
    }

    write_log("System", "SERVER ONLINE");
    printf("[System] The Wired is online on portt %d\n", SERVER_PORT);

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_fd,        &rfds);
        FD_SET(shutdown_pipe[0], &rfds);

        int maxfd = (server_fd > shutdown_pipe[0])
                  ? server_fd : shutdown_pipe[0];

        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(shutdown_pipe[0], &rfds)) {
            printf("[System] Emergency shutdown signal received.\n");
            pthread_mutex_lock(&clientMutex);
            for (int i = 0; i < 64; i++) {
                if (clients[i].cekslot) close(clients[i].fd);
            }
            pthread_mutex_unlock(&clientMutex);
            break;
        }

        if (FD_ISSET(server_fd, &rfds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int new_fd = accept(server_fd,
                                (struct sockaddr *)&cli_addr, &cli_len);
            if (new_fd < 0) continue;

            pthread_mutex_lock(&clientMutex);
            int slot = find_slot();
            if (slot < 0) {
                pthread_mutex_unlock(&clientMutex);
                close(new_fd);
                continue;
            }
            clients[slot].fd     = new_fd;
            clients[slot].cekslot = 1;
            clients[slot].cekAdmin = 0;
            clients[slot].name[0]  = '\0';
            pthread_mutex_unlock(&clientMutex);

            int *slot_ptr = malloc(sizeof(int));
            *slot_ptr = slot;
            pthread_t tid;
            if (pthread_create(&tid, NULL, handle_client_thread, slot_ptr) != 0) {
                perror("pthread_create");
                free(slot_ptr);
                close(new_fd);
                pthread_mutex_lock(&clientMutex);
                clients[slot].cekslot = 0;
                pthread_mutex_unlock(&clientMutex);
            }
        }
    }

    close(server_fd);
    close(shutdown_pipe[0]);
    close(shutdown_pipe[1]);
    printf("[System] The Wired has gone offline.\n");
    return 0;
}