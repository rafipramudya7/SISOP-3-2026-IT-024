
#include "arena.h"

static int     shmid, msgid, semid;
static SharedArena *arena;

#define SAVE_FILE "eterion_save.dat"

void save_data(void) {
    FILE *f = fopen(SAVE_FILE, "wb");
    if (!f) return;
    fwrite(&arena->player_count, sizeof(int), 1, f);
    fwrite(arena->players, sizeof(Player), arena->player_count, f);
    fclose(f);
}

void load_data(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return;
    fread(&arena->player_count, sizeof(int), 1, f);
    fread(arena->players, sizeof(Player), arena->player_count, f);
    fclose(f);
    for (int i = 0; i < arena->player_count; i++) {
        arena->players[i].online    = 0;
        arena->players[i].in_battle = 0;
        arena->players[i].battle_slot = -1;
    }
}

int find_player(const char *name) {
    for (int i = 0; i < arena->player_count; i++)
        if (strcmp(arena->players[i].username, name) == 0) return i;
    return -1;
}

void send_resp(long pid, const char *msg) {
    ArenaMsg m;
    m.mtype = pid + MSG_RESPONSE;
    strncpy(m.data, msg, 255);
    msgsnd(msgid, &m, sizeof(m.data), 0);
}

int calc_dmg(Player *p) {
    int bonus = (p->weapon_idx >= 0) ? WEAPONS[p->weapon_idx].bonus_dmg : 0;
    return BASE_DMG + (p->xp / 50) + bonus;
}
int calc_hp(Player *p) { return BASE_HP + (p->xp / 10); }

void add_history(Player *p, const char *opp, const char *res, int xp) {
    int i = p->history_count % MAX_HISTORY;
    strncpy(p->history[i].opponent,  opp, MAX_NAME-1);
    strncpy(p->history[i].result,    res, 7);
    p->history[i].xp_gained = xp;
    time_t now = time(NULL);
    strftime(p->history[i].timestamp, 31, "%Y-%m-%d %H:%M", localtime(&now));
    p->history_count++;
}

void end_battle(int slot, int winner_side) {
    Battle *b = &arena->battles[slot];
    if (!b->active) return;
    int w = (winner_side == 1) ? b->p1_idx : b->p2_idx;
    int l = (winner_side == 1) ? b->p2_idx : b->p1_idx;
    if (w >= 0) {
        arena->players[w].xp   += 50;
        arena->players[w].gold += 120;
        arena->players[w].level = (arena->players[w].xp / 100) + 1;
        add_history(&arena->players[w], (l<0)?"BOT":arena->players[l].username, "WIN", 50);
    }
    if (l >= 0) {
        arena->players[l].xp   += 15;
        arena->players[l].gold += 30;
        arena->players[l].level = (arena->players[l].xp / 100) + 1;
        add_history(&arena->players[l], (w<0)?"BOT":arena->players[w].username, "LOSE", 15);
    }
    b->winner = winner_side;
    b->active = 0;
    if (b->p1_idx >= 0) { arena->players[b->p1_idx].in_battle = 0; arena->players[b->p1_idx].online = 1; }
    if (b->p2_idx >= 0) { arena->players[b->p2_idx].in_battle = 0; arena->players[b->p2_idx].online = 1; }
    save_data();
}
typedef struct { int slot; } BotArg;
static const char RPOOL[] = "abcdefghijklmnopqrstuvwxyz";

void *bot_thread(void *arg) {
    int slot = ((BotArg*)arg)->slot;
    free(arg);
    Battle *b = &arena->battles[slot];
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    while (b->active && b->p2_idx == -1) {
        int jeda = 3 - (b->react_round / 5);
        if (jeda < 1) jeda = 1;
        sleep(jeda);
        if (!b->active) break;

        double window = 2.5 - (b->react_round / 3) * 0.3;
        if (window < 1.0) window = 1.0;

        b->react_char     = RPOOL[rand() % 26];
        b->react_target   = 1;
        b->react_time     = time(NULL);
        b->react_answered = 0;

        usleep((useconds_t)(window * 1000000));
        if (!b->active) break;

        if (!b->react_answered) {
            b->p1_hp -= b->p2_dmg;
            if (b->p1_hp <= 0) { b->react_target = 0; end_battle(slot, 2); break; }
        }
        b->react_target   = 0;
        b->react_answered = 0;
        b->react_round++;
    }
    return NULL;
}

void spawn_bot(int slot) {
    BotArg *ba = malloc(sizeof(BotArg));
    ba->slot = slot;
    pthread_t tid;
    pthread_create(&tid, NULL, bot_thread, ba);
    pthread_detach(tid);
}
void handle_register(ArenaMsg *m, long pid) {
    char u[MAX_NAME], p[MAX_PASS];
    sscanf(m->data, "%s %s", u, p);
    sem_lock(semid);
    if (find_player(u) >= 0)          { sem_unlock(semid); send_resp(pid, "ERR Username sudah ada"); return; }
    if (arena->player_count >= MAX_PLAYERS) { sem_unlock(semid); send_resp(pid, "ERR Server penuh"); return; }
    Player *pl = &arena->players[arena->player_count++];
    strncpy(pl->username, u, MAX_NAME-1);
    strncpy(pl->password, p, MAX_PASS-1);
    pl->gold=150; pl->level=1; pl->xp=0; pl->weapon_idx=-1;
    pl->online=0; pl->in_battle=0; pl->battle_slot=-1; pl->history_count=0;
    save_data();
    sem_unlock(semid);
    send_resp(pid, "OK Register berhasil");
}

void handle_login(ArenaMsg *m, long pid) {
    char u[MAX_NAME], p[MAX_PASS];
    sscanf(m->data, "%s %s", u, p);
    sem_lock(semid);
    int i = find_player(u);
    if (i < 0 || strcmp(arena->players[i].password, p) != 0) { sem_unlock(semid); send_resp(pid, "ERR Username/password salah"); return; }
    if (arena->players[i].online)                             { sem_unlock(semid); send_resp(pid, "ERR Akun sedang online"); return; }
    arena->players[i].online = 1;
    sem_unlock(semid);
    char resp[256];
    Player *pl = &arena->players[i];
    snprintf(resp,255,"OK %d %s %d %d %d %d", i, pl->username, pl->gold, pl->level, pl->xp, pl->weapon_idx);
    send_resp(pid, resp);
}

void handle_logout(ArenaMsg *m, long pid) {
    int i; sscanf(m->data, "%d", &i);
    sem_lock(semid);
    if (i >= 0) arena->players[i].online = 0;
    sem_unlock(semid);
    send_resp(pid, "OK Logout");
}
void handle_queue(ArenaMsg *m, long pid) {
    int idx; sscanf(m->data, "%d", &idx);
    sem_lock(semid);

    // Cek ada yang waiting di shm 
    for (int i = 0; i < MAX_BATTLES; i++) {
        Battle *b = &arena->battles[i];
        if (b->active && b->p2_idx == -2 && b->p1_idx != idx) {
            b->p2_idx     = idx;
            b->p2_hp      = b->p2_max_hp = calc_hp(&arena->players[idx]);
            b->p2_dmg     = calc_dmg(&arena->players[idx]);
            arena->players[idx].in_battle   = 1;
            arena->players[idx].battle_slot = i;
            sem_unlock(semid);
            char r[256];
            snprintf(r,255,"MATCH %d %d %d %d %d %d %d", i,
                     b->p1_idx,
                     b->p2_hp, b->p2_max_hp, b->p2_dmg,
                     b->p1_hp, b->p1_max_hp);
            send_resp(pid, r);
            return;
        }
    }
    int slot = -1;
    for (int i = 0; i < MAX_BATTLES; i++)
        if (!arena->battles[i].active) { slot = i; break; }
    if (slot < 0) { sem_unlock(semid); send_resp(pid, "ERR Arena penuh"); return; }

    Battle *b = &arena->battles[slot];
    memset(b, 0, sizeof(Battle));
    b->active    = 1;
    b->p1_idx    = idx;
    b->p2_idx    = -2;  
    b->p1_hp     = b->p1_max_hp = calc_hp(&arena->players[idx]);
    b->p1_dmg    = calc_dmg(&arena->players[idx]);
    arena->players[idx].in_battle   = 1;
    arena->players[idx].battle_slot = slot;
    sem_unlock(semid);
    char r[64];
    snprintf(r,63,"WAIT %d", slot);
    send_resp(pid, r);
}

void handle_bot(ArenaMsg *m, long pid) {
    int idx, slot; sscanf(m->data, "%d %d", &idx, &slot);
    sem_lock(semid);
    Battle *b = &arena->battles[slot];
    if (!b->active || b->p2_idx != -2) { sem_unlock(semid); return; }
    b->p2_idx    = -1;
    b->p2_hp     = b->p2_max_hp = BASE_HP + 20;
    b->p2_dmg    = BASE_DMG + 3;
    sem_unlock(semid);
    spawn_bot(slot);
    char r[256];
    snprintf(r,255,"OK %d -1 %d %d %d %d %d", slot,
             b->p1_hp, b->p1_max_hp, b->p1_dmg,
             b->p2_hp, b->p2_max_hp);
    send_resp(pid, r);
}

void handle_attack(ArenaMsg *m, long pid) {
    int pidx, slot; sscanf(m->data, "%d %d", &pidx, &slot);
    Battle *b = &arena->battles[slot];
    if (!b->active) return;
    sem_lock(semid);
    if (b->p1_idx == pidx) { b->p2_hp -= b->p1_dmg; if (b->p2_hp<=0){sem_unlock(semid);end_battle(slot,1);return;} }
    else                   { b->p1_hp -= b->p2_dmg; if (b->p1_hp<=0){sem_unlock(semid);end_battle(slot,2);return;} }
    sem_unlock(semid);
}
void handle_ultimate(ArenaMsg *m, long pid) {
    int pidx, slot; sscanf(m->data, "%d %d", &pidx, &slot);
    Battle *b = &arena->battles[slot];
    if (!b->active) return;

    sem_lock(semid);
    if (b->p1_idx == pidx) {
        if (!b->p1_ult_used && arena->players[pidx].weapon_idx >= 0) {
            b->p2_hp -= (b->p1_dmg * 3);
            b->p1_ult_used = 1;
        }
    } else if (b->p2_idx == pidx) {
        if (!b->p2_ult_used && arena->players[pidx].weapon_idx >= 0) {
            b->p1_hp -= (b->p2_dmg * 3);
            b->p2_ult_used = 1;
        }
    }
    
    // Cek kematian setelah ultimate
    if (b->p1_hp <= 0) { sem_unlock(semid); end_battle(slot, 2); return; }
    if (b->p2_hp <= 0) { sem_unlock(semid); end_battle(slot, 1); return; }
    sem_unlock(semid);
}

void handle_react(ArenaMsg *m, long pid) {
    int pidx, slot; char pressed;
    sscanf(m->data, "%d %d %c", &pidx, &slot, &pressed);
    Battle *b = &arena->battles[slot];
    if (!b->active) return;
    sem_lock(semid);
    if (b->react_target && !b->react_answered && pressed == b->react_char) {
        b->react_answered = 1;
        int dmg = (b->p1_idx == pidx) ? b->p1_dmg : b->p2_dmg;
        if (b->p1_idx == pidx) b->p2_hp -= dmg;
        else                   b->p1_hp -= dmg;
        if (b->p2_hp<=0){sem_unlock(semid);end_battle(slot,1);return;}
        if (b->p1_hp<=0){sem_unlock(semid);end_battle(slot,2);return;}
    }
    sem_unlock(semid);
}

void handle_buy(ArenaMsg *m, long pid) {
    int pidx, widx; sscanf(m->data, "%d %d", &pidx, &widx);
    if (widx < 0 || widx >= WEAPON_COUNT) { send_resp(pid,"ERR Tidak valid"); return; }
    sem_lock(semid);
    Player *p = &arena->players[pidx];
    if (p->gold < WEAPONS[widx].price)                                              { sem_unlock(semid); send_resp(pid,"ERR Gold tidak cukup"); return; }
    if (p->weapon_idx>=0 && WEAPONS[widx].bonus_dmg<=WEAPONS[p->weapon_idx].bonus_dmg) { sem_unlock(semid); send_resp(pid,"ERR Senjata lebih lemah"); return; }
    p->gold -= WEAPONS[widx].price;
    p->weapon_idx = widx;
    save_data();
    sem_unlock(semid);
    char r[128]; snprintf(r,127,"OK Beli %s! Gold: %d", WEAPONS[widx].name, p->gold);
    send_resp(pid, r);
}
void cleanup(int sig) {
    (void)sig;
    printf("\n[ORION] Shutdown.\n");
    shmdt(arena);
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(msgid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    exit(0);
}

int main(void) {
    signal(SIGINT, cleanup); signal(SIGTERM, cleanup);
    shmid = shmget(SHM_KEY, sizeof(SharedArena), IPC_CREAT|0666);
    arena = shmat(shmid, NULL, 0);
    memset(arena, 0, sizeof(SharedArena));
    msgid = msgget(MSG_KEY, IPC_CREAT|0666);
    semid = semget(SEM_KEY, 1, IPC_CREAT|0666);
    semctl(semid, 0, SETVAL, 1);
    load_data();

    printf("\n");
    printf("  ██████╗ ██████╗ ██╗ ██████╗ ███╗   ██╗\n");
    printf("  ██╔══██╗██╔══██╗██║██╔═══██╗████╗  ██║\n");
    printf("  ██║  ██║██████╔╝██║██║   ██║██╔██╗ ██║\n");
    printf("  ██║  ██║██╔══██╗██║██║   ██║██║╚██╗██║\n");
    printf("  ██████╔╝██║  ██║██║╚██████╔╝██║ ╚████║\n");
    printf("  ╚═════╝ ╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝\n");
    printf("\n  ⚔️  Battle of Eterion — Server Online\n");
    printf("  Players: %d | Ready.\n\n", arena->player_count);

    ArenaMsg msg;
    while (1) {
        if (msgrcv(msgid, &msg, sizeof(msg.data), 0, 0) < 0) continue;
        long cmd = msg.mtype % 10;
        long pid = msg.mtype / 10;
        switch (cmd) {
            case MSG_ULTIMATE: handle_ultimate(&msg, pid); break;
            case MSG_REGISTER:   handle_register(&msg, pid); break;
            case MSG_LOGIN:      handle_login(&msg, pid);    break;
            case MSG_LOGOUT:     handle_logout(&msg, pid);   break;
            case MSG_BATTLE_REQ: handle_queue(&msg, pid);    break;
            case MSG_BOT_REQ:    handle_bot(&msg, pid);      break;
            case MSG_ATTACK:     handle_attack(&msg, pid);   break;
            case MSG_REACT_OK:   handle_react(&msg, pid);    break;
            case MSG_BUY:        handle_buy(&msg, pid);      break;
        }
    }
    return 0;
}