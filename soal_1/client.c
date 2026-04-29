
#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

static int sockfd = -1;
static int is_admin = 0; 

static int exit_pipe[2];

static void *recv_thread_func(void *arg)
{
    (void)arg;
    Packet pkt;

    while (1)
    {
        ssize_t n = recv(sockfd, &pkt, sizeof(pkt), 0);
        if (n <= 0)
        {
            printf("\n[System] Connection to The Wired lost.\n");
            char sig = 1;
            write(exit_pipe[1], &sig, 1);
            break;
        }
        switch (pkt.type)
        {
        case 2:
            printf("%s\n", pkt.body);
            if (strstr(pkt.body, "Disconnecting from The Wired"))
            {
                char sig = 1;
                write(exit_pipe[1], &sig, 1);
            }
            break;

        case MSG_CHAT:
            printf("%s\n", pkt.body);
            break;

        case MSG_RPC_RES:

            printf("[RPC Result] %s\n", pkt.body);
            if (strstr(pkt.body, "EMERGENCY SHUTDOWN"))
            {
                char sig = 1;
                write(exit_pipe[1], &sig, 1);
            }
            break;
        case MSG_AUTH:
            printf("%s\n", pkt.body);
            break;

        default:
            break;
        }
        printf("> ");
        fflush(stdout);
    }

    pthread_detach(pthread_self());
    return NULL;
}

static int send_pkt(MsgType type, const char *body, int rpc_cmd)
{
    Packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = type;
    pkt.rpc_cmd = rpc_cmd;
    // strncpy(pkt.sender, my_name, 1000 - 1);
    if (body)
        strncpy(pkt.body, body, 999);
    return (int)send(sockfd, &pkt, sizeof(pkt), 0);
}

static int do_register(void)
{
    Packet pkt;

    while (1)
    {
        printf("Enter your name: ");
        fflush(stdout);
        char my_name[1000];
        if (!fgets(my_name, 1000, stdin))
            return -1;
        my_name[strcspn(my_name, "\n")] = '\0';
        if (strlen(my_name) == 0)
            continue;

        send_pkt(MSG_SYSTEM, my_name, 0);

        ssize_t n = recv(sockfd, &pkt, sizeof(pkt), 0);
        if (n <= 0)
            return -1;

        if (pkt.type == MSG_SYSTEM &&
            strstr(pkt.body, "already synchronized"))
        {
            printf("%s\n", pkt.body);
            continue;
        }

        if (pkt.type == MSG_AUTH &&
            strcmp(pkt.body, "PASSWORD_REQUIRED") == 0)
        {
            printf("Enter Password: ");
            fflush(stdout);
            char pw[1000];
            if (!fgets(pw, sizeof(pw), stdin))
                return -1;
            pw[strcspn(pw, "\n")] = '\0';
            send_pkt(MSG_AUTH, pw, 0);
            n = recv(sockfd, &pkt, sizeof(pkt), 0);
            if (n <= 0)
                return -1;

            if (strcmp(pkt.body, "AUTH_OK") == 0)
            {
                printf("\n[System] Authentication Successful. "
                       "Granted Admin privileges.\n");
                is_admin = 1;
                return 0;
            }
            else
            {
                printf("[System] Authentication Failed. Disconnecting.\n");
                return -1;
            }
        }
        if (pkt.type == MSG_SYSTEM)
        {
            printf("%s\n", pkt.body);
            return 0;
        }
    }
}
static void show_admin_console(void)
{
    printf("\n=== THE KNIGHTS CONSOLE ===\n");
    printf("1. Check Active Entites (Users)\n");
    printf("2. Check Server Uptime\n");
    printf("3. Execute Emergency Shutdown\n");
    printf("4. Disconnect\n");
}

int main(void)
{
    if (pipe(exit_pipe) < 0)
    {
        perror("pipe");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT)};
    inet_pton(AF_INET, SERVER_HOST, &srv.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&srv, sizeof(srv)) < 0)
    {
        perror("connect");
        return 1;
    }
    if (do_register() < 0)
    {
        close(sockfd);
        return 1;
    }

    pthread_t recv_tid;
    if (pthread_create(&recv_tid, NULL, recv_thread_func, NULL) != 0)
    {
        perror("pthread_create");
        close(sockfd);
        return 1;
    }

    if (is_admin)
    {
        while (1)
        {
            show_admin_console();
            printf("Command >> ");
            fflush(stdout);
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(STDIN_FILENO, &rfds);
            FD_SET(exit_pipe[0], &rfds);
            int maxfd = exit_pipe[0];
            if (STDIN_FILENO > maxfd)
                maxfd = STDIN_FILENO;

            if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0)
                break;

            if (FD_ISSET(exit_pipe[0], &rfds))
                break;

            if (FD_ISSET(STDIN_FILENO, &rfds))
            {
                char buf[16];
                if (!fgets(buf, sizeof(buf), stdin))
                    break;
                int choice = atoi(buf);

                if (choice == 1)
                {
                    send_pkt(MSG_RPC_REQ, "", RPC_GET_USERS);
                }
                else if (choice == 2)
                {
                    send_pkt(MSG_RPC_REQ, "", RPC_GET_UPTIME);
                }
                else if (choice == 3)
                {
                    send_pkt(MSG_RPC_REQ, "", RPC_SHUTDOWN);
                    sleep(1);
                    break;
                }
                else if (choice == 4 ||
                         (buf[0] == '^' && buf[1] == 'C'))
                {
                    send_pkt(MSG_EXIT, "/exit", 0);
                    break;
                }
            }
        }
    }
    else
    {
        printf("> ");
        fflush(stdout);

        while (1)
        {

            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(STDIN_FILENO, &rfds);
            FD_SET(exit_pipe[0], &rfds);
            int maxfd = exit_pipe[0];
            if (STDIN_FILENO > maxfd)
                maxfd = STDIN_FILENO;

            if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0)
                break;
            if (FD_ISSET(exit_pipe[0], &rfds))
                break;

            if (FD_ISSET(STDIN_FILENO, &rfds))
            {
                char buf[1000];
                if (!fgets(buf, sizeof(buf), stdin))
                    break;
                buf[strcspn(buf, "\n")] = '\0';
                if (strlen(buf) == 0)
                {
                    printf("> ");
                    fflush(stdout);
                    continue;
                }
                if (strcmp(buf, "/exit") == 0)
                {
                    send_pkt(MSG_EXIT, "/exit", 0);
                    sleep(1); 
                    break;
                }
                send_pkt(MSG_CHAT, buf, 0);
                printf("> ");
                fflush(stdout);
            }
        }
    }

    close(sockfd);
    close(exit_pipe[0]);
    close(exit_pipe[1]);
    printf("[System] Disconnecting from The Wired...\n");
    return 0;
}