#include "arena.h"

static int msgid, shmid, semid;
static SharedArena *arena;
static pid_t mypid;
void send_cmd(int cmd, const char *data)
{
    ArenaMsg m;
    m.mtype = (long)mypid * 10 + cmd;
    strncpy(m.data, data, 255);
    msgsnd(msgid, &m, sizeof(m.data), 0);
}

char *wait_response(void)
{
    static ArenaMsg m;
    m.mtype = (long)mypid + MSG_RESPONSE;
    msgrcv(msgid, &m, sizeof(m.data), (long)mypid + MSG_RESPONSE, 0);
    return m.data;
}
#define CLR "\033[2J\033[H"
#define BOLD "\033[1m"
#define YEL "\033[33m"
#define CYN "\033[36m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define MAG "\033[35m"
#define RST "\033[0m"

void draw_hp_bar(int hp, int maxhp, int width)
{
    int filled = (maxhp > 0) ? (hp * width / maxhp) : 0;
    if (filled < 0)
        filled = 0;
    printf("[");
    for (int i = 0; i < width; i++)
    {
        if (i < filled)
        {
            if (filled > width / 2)
                printf(GRN "█" RST);
            else if (filled > width / 4)
                printf(YEL "█" RST);
            else
                printf(RED "█" RST);
        }
        else
            printf("░");
    }
    printf("] %d/%d", hp < 0 ? 0 : hp, maxhp);
}

void print_banner(void)
{
    printf(CLR);
    printf(CYN);
    printf("  ███████╗████████╗███████╗██████╗ ██╗ ██████╗ ███╗   ██╗\n");
    printf("  ██╔════╝╚══██╔══╝██╔════╝██╔══██╗██║██╔═══██╗████╗  ██║\n");
    printf("  █████╗     ██║   █████╗  ██████╔╝██║██║   ██║██╔██╗ ██║\n");
    printf("  ██╔══╝     ██║   ██╔══╝  ██╔══██╗██║██║   ██║██║╚██╗██║\n");
    printf("  ███████╗   ██║   ███████╗██║  ██║██║╚██████╔╝██║ ╚████║\n");
    printf("  ╚══════╝   ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝\n");
    printf(RST);
    printf(YEL "              ⚔️  Battle of Eterion  ⚔️\n" RST);
    printf("  ─────────────────────────────────────────────────────\n\n");
}
int kbhit(void)
{
    fd_set fds;
    struct timeval tv = {0, 0};
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

typedef struct
{
    int slot;
    int pidx;
    int is_bot;
    int running;
    char log1[128];
    char log2[128];
    time_t last_atk;
} BattleCtx;

static BattleCtx bctx;

void draw_battle(void)
{
    Battle *b = &arena->battles[bctx.slot];
    Player *me = &arena->players[bctx.pidx];

    int my_hp = (b->p1_idx == bctx.pidx) ? b->p1_hp : b->p2_hp;
    int my_max = (b->p1_idx == bctx.pidx) ? b->p1_max_hp : b->p2_max_hp;
    int opp_hp = (b->p1_idx == bctx.pidx) ? b->p2_hp : b->p1_hp;
    int opp_max = (b->p1_idx == bctx.pidx) ? b->p2_max_hp : b->p1_max_hp;
    const char *opp_name = bctx.is_bot ? "B.O.T" : (b->p1_idx == bctx.pidx ? (b->p2_idx >= 0 ? arena->players[b->p2_idx].username : "BOT") : arena->players[b->p1_idx].username);

    printf(CLR);
    printf(CYN "  ╔══════════════════════════════════════╗\n" RST);
    printf(CYN "  ║" RST BOLD "          ⚔  ARENA BATTLE  ⚔         " CYN "║\n" RST);
    printf(CYN "  ╚══════════════════════════════════════╝\n\n" RST);

    printf("  " RED "▸ %s" RST "  (Lv%d)\n",
           opp_name,
           bctx.is_bot ? 1 : (b->p2_idx >= 0 ? arena->players[b->p2_idx].level : 1));
    printf("  HP  ");
    draw_hp_bar(opp_hp, opp_max, 20);
    printf("\n\n");

    printf("  " GRN "▸ %s" RST "  (Lv%d)\n", me->username, me->level);
    printf("  HP  ");
    draw_hp_bar(my_hp, my_max, 20);
    printf("\n\n");

    printf("  ─────────────────────────────────────────\n");

    printf("\n");
    if (b->react_target != 0 && !b->react_answered)
    {
        time_t now = time(NULL);
        double window = 2.5 - (b->react_round / 3) * 0.3;
        if (window < 1.0)
            window = 1.0;
        double left = window - difftime(now, b->react_time);
        if (left < 0)
            left = 0;
        if (left > window * 0.6)
            printf(GRN "  ╔══════════════════════════════════╗\n"
                       "  ║  Tekan  [ %c ]  sekarang!      ║\n"
                       "  ║  Waktu: %.1fs                     ║\n"
                       "  ╚══════════════════════════════════╝\n" RST,
                   b->react_char, left);
        else if (left > window * 0.3)
            printf(YEL "  ╔══════════════════════════════════╗\n"
                       "  ║  ⚠  Tekan  [ %c ]  cepat!        ║\n"
                       "  ║  Waktu: %.1fs                     ║\n"
                       "  ╚══════════════════════════════════╝\n" RST,
                   b->react_char, left);
        else
            printf(RED "  ╔══════════════════════════════════╗\n"
                       "  ║  SEKARANG!! [ %c ] !!         ║\n"
                       "  ║  Waktu: %.1fs                     ║\n"
                       "  ╚══════════════════════════════════╝\n" RST,
                   b->react_char, left);
    }
    else
    {
        printf("  [" GRN "a" RST "] Normal Attack ");
        if (me->weapon_idx >= 0)
        {
            int ult_used = (b->p1_idx == bctx.pidx) ? b->p1_ult_used : b->p2_ult_used;
            if (!ult_used)
                printf("  [" MAG "u" RST "] " BOLD "ULTIMATE (3x DMG)" RST);
        }
        printf("   [q] Quit\n");
    }

    printf("\n  Combat Log:\n");
    if (bctx.log1[0])
        printf("  " CYN "> %s\n" RST, bctx.log1);
    if (bctx.log2[0])
        printf("  " CYN "> %s\n" RST, bctx.log2);

    double atk_cd = difftime(time(NULL), bctx.last_atk);
    printf("\n  CD: Atk(%s)\n",
           atk_cd >= 1.0 ? GRN "READY" RST : YEL "1.0s" RST);
    fflush(stdout);
}

void push_log(const char *msg)
{
    strncpy(bctx.log2, bctx.log1, 127);
    strncpy(bctx.log1, msg, 127);
}

void *battle_render_thread(void *arg)
{
    (void)arg;
    while (bctx.running)
    {
        draw_battle();
        usleep(200000);
    }
    return NULL;
}
void do_battle(int slot, int pidx, int opp_idx)
{
    memset(&bctx, 0, sizeof(bctx));
    bctx.slot = slot;
    bctx.pidx = pidx;
    bctx.is_bot = (opp_idx == -1);
    bctx.running = 1;
    bctx.last_atk = time(NULL) - 2;

    set_raw_mode();

    pthread_t render_tid;
    pthread_create(&render_tid, NULL, battle_render_thread, NULL);

    Battle *b = &arena->battles[slot];
    while (bctx.running)
    {
        if (!b->active)
        {
            bctx.running = 0;
            break;
        }

        if (kbhit())
        {
            char c = 0;
            read(STDIN_FILENO, &c, 1);

            if (c == 'q')
            {
                bctx.running = 0;
            }
            else if (b->react_target != 0 && !b->react_answered)
            {
                char buf[64];
                snprintf(buf, 63, "%d %d %c", pidx, slot, c);
                send_cmd(MSG_REACT_OK, buf);
                int dmg = (b->p1_idx == pidx) ? b->p1_dmg : b->p2_dmg;
                char logmsg[80];
                if (c == b->react_char)
                    snprintf(logmsg, 79, "⚡ [%c] COUNTER! -%d dmg ke lawan!", c, dmg);
                else
                    snprintf(logmsg, 79, "❌ [%c] Salah! Harusnya [%c]", c, b->react_char);
                push_log(logmsg);
            }
            else if (c == 'a')
            {
                double elapsed = difftime(time(NULL), bctx.last_atk);
                if (elapsed >= 1.0)
                {
                    bctx.last_atk = time(NULL);
                    char buf[64];
                    snprintf(buf, 63, "%d %d", pidx, slot);
                    send_cmd(MSG_ATTACK, buf);
                    int dmg = (b->p1_idx == pidx) ? b->p1_dmg : b->p2_dmg;
                    char logmsg[64];
                    snprintf(logmsg, 63, "Kamu menyerang! -%d HP", dmg);
                    push_log(logmsg);
                }
                else
                {
                    push_log("Cooldown! Tunggu sebentar...");
                }
            }
            else if (c == 'u')
            {
                Player *me = &arena->players[pidx];
                int ult_used = (b->p1_idx == pidx) ? b->p1_ult_used : b->p2_ult_used;

                if (me->weapon_idx >= 0 && !ult_used)
                {
                    char buf[64];
                    snprintf(buf, 63, "%d %d", pidx, slot);
                    send_cmd(MSG_ULTIMATE, buf);

                    int dmg = ((b->p1_idx == pidx) ? b->p1_dmg : b->p2_dmg) * 3;
                    char logmsg[80];
                    snprintf(logmsg, 79, "🔥 ULTIMATE!! " MAG "%s" RST " meledak: -%d HP!",
                             WEAPONS[me->weapon_idx].name, dmg);
                    push_log(logmsg);
                }
                else if (me->weapon_idx < 0)
                {
                    push_log("Butuh senjata di Armory untuk Ultimate!");
                }
                else
                {
                    push_log("Ultimate sudah digunakan!");
                }
            }
        }
        usleep(50000);
    }

    pthread_join(render_tid, NULL);
    restore_terminal();

    printf(CLR);
    if (b->winner == 0)
    {
        printf(YEL "\n  ⚠ Pertempuran berakhir.\n" RST);
    }
    else
    {
        int my_side = (b->p1_idx == pidx) ? 1 : 2;
        if (b->winner == my_side)
        {
            printf(GRN "\n  ╔══════════════════╗\n");
            printf("  ║   VICTORY!   ║\n");
            printf("  ╚══════════════════╝\n" RST);
            printf("  +50 XP  +120 Gold\n");
        }
        else
        {
            printf(RED "\n  ╔══════════════════╗\n");
            printf("  ║   DEFEATED   ║\n");
            printf("  ╚══════════════════╝\n" RST);
            printf("  +15 XP  +30 Gold\n");
        }
    }
    printf("\n  Tekan ENTER untuk kembali...\n");
    restore_terminal();
    getchar();
}

void menu_armory(int pidx)
{
    print_banner();
    Player *p = &arena->players[pidx];
    printf(BOLD "  ⚔  ARMORY\n\n" RST);
    printf("  Gold kamu: " YEL "%d" RST "\n\n", p->gold);
    printf("  %-3s %-16s %-8s %-6s\n", "No", "Senjata", "Bonus", "Harga");
    printf("  ─────────────────────────────────\n");
    for (int i = 0; i < WEAPON_COUNT; i++)
    {
        char mark = (p->weapon_idx == i) ? '*' : ' ';
        printf("  %c%d. %-14s +%-6d %d gold\n",
               mark, i + 1,
               WEAPONS[i].name,
               WEAPONS[i].bonus_dmg,
               WEAPONS[i].price);
    }
    printf("\n  0. Kembali\n  > Pilih senjata: ");
    fflush(stdout);

    int choice;
    scanf("%d", &choice);
    if (choice < 1 || choice > WEAPON_COUNT)
        return;

    char buf[64];
    snprintf(buf, 63, "%d %d", pidx, choice - 1);
    send_cmd(MSG_BUY, buf);
    char *resp = wait_response();
    printf("\n  %s\n", resp + 3);
    sleep(1);
}

void menu_history(int pidx)
{
    print_banner();
    Player *p = &arena->players[pidx];
    printf(BOLD "  📜  RIWAYAT PERTEMPURAN\n\n" RST);
    if (p->history_count == 0)
    {
        printf("  Belum ada riwayat.\n");
    }
    else
    {
        int total = p->history_count < MAX_HISTORY ? p->history_count : MAX_HISTORY;
        printf("  %-20s %-10s %-5s %-20s\n", "Lawan", "Hasil", "XP", "Waktu");
        printf("  ─────────────────────────────────────────────────────\n");
        for (int i = 0; i < total; i++)
        {
            HistoryEntry *h = &p->history[i];
            const char *col = strcmp(h->result, "WIN") == 0 ? GRN : RED;
            printf("  %-20s %s%-10s" RST " %-5d %-20s\n",
                   h->opponent, col, h->result, h->xp_gained, h->timestamp);
        }
    }
    printf("\n  Tekan ENTER untuk kembali...\n");
    getchar();
    getchar();
}

void menu_lobby(int pidx)
{
    while (1)
    {
        Player *p = &arena->players[pidx];
        print_banner();
        printf("  ┌─────────────────────────────────────┐\n");
        printf("  │ Name : %-12s   Lv : %-5d  │\n", p->username, p->level);
        printf("  │ Gold : %-12d   XP : %-5d  │\n", p->gold, p->xp);
        if (p->weapon_idx >= 0)
            printf("  │ Senjata: %-28s│\n", WEAPONS[p->weapon_idx].name);
        printf("  └─────────────────────────────────────┘\n\n");
        printf("  1. Battle\n");
        printf("  2. Armory\n");
        printf("  3. Riwayat\n");
        printf("  4. Logout\n\n");
        printf("  > Pilih: ");
        fflush(stdout);

        int c;
        scanf("%d", &c);
        if (c == 1)
        {
            printf("\n  " YEL "Mencari lawan" RST);
            fflush(stdout);

            char buf[64];
            snprintf(buf, 63, "%d", pidx);
            send_cmd(MSG_BATTLE_REQ, buf);
            char *resp = wait_response();

            if (strncmp(resp, "ERR", 3) == 0)
            {
                printf("\n  " RED "%s\n" RST, resp + 4);
                sleep(2);
                goto lobby_continue;
            }

            if (strncmp(resp, "MATCH", 5) == 0)
            {
                int slot, opp_idx, myhp, mymax, mydmg, opphp, oppmax;
                sscanf(resp + 6, "%d %d %d %d %d %d %d",
                       &slot, &opp_idx, &myhp, &mymax, &mydmg, &opphp, &oppmax);
                printf("\n  " GRN "Lawan ditemukan! Mulai!\n" RST);
                sleep(1);
                do_battle(slot, pidx, opp_idx);
                goto lobby_continue;
            }

            int slot;
            sscanf(resp + 5, "%d", &slot);
            int found = 0;
            for (int t = 0; t < 20; t++)
            {
                printf(".");
                fflush(stdout);
                sleep(1);
                if (arena->battles[slot].p2_idx >= 0)
                {
                    Battle *b = &arena->battles[slot];
                    printf("\n  " GRN "Lawan ditemukan! Mulai!\n" RST);
                    sleep(1);
                    do_battle(slot, pidx, b->p2_idx);
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                printf("\n  " YEL "Tidak ada lawan. Melawan BOT!\n" RST);
                char botbuf[32];
                snprintf(botbuf, 31, "%d %d", pidx, slot);
                send_cmd(MSG_BOT_REQ, botbuf);
                char *br = wait_response();
                if (strncmp(br, "OK", 2) == 0)
                {
                    int s, oi, p1h, p1m, p1d, p2h, p2m;
                    sscanf(br + 3, "%d %d %d %d %d %d %d", &s, &oi, &p1h, &p1m, &p1d, &p2h, &p2m);
                    sleep(1);
                    do_battle(s, pidx, -1);
                }
            }
        lobby_continue:;
        }
        else if (c == 2)
        {
            menu_armory(pidx);
        }
        else if (c == 3)
        {
            menu_history(pidx);
        }
        else if (c == 4)
        {
            char buf[16];
            snprintf(buf, 15, "%d", pidx);
            send_cmd(MSG_LOGOUT, buf);
            wait_response();
            printf("\n  Sampai jumpa, " GRN "%s" RST "!\n\n", p->username);
            sleep(1);
            return;
        }
    }
}
void do_register(void)
{
    char uname[MAX_NAME], pass[MAX_PASS];
    print_banner();
    printf("  ─── REGISTER ───\n\n");
    printf("  Username : ");
    scanf("%31s", uname);
    printf("  Password : ");
    scanf("%31s", pass);

    char buf[128];
    snprintf(buf, 127, "%s %s", uname, pass);
    send_cmd(MSG_REGISTER, buf);
    char *resp = wait_response();
    if (strncmp(resp, "OK", 2) == 0)
        printf("\n  " GRN "✔ Berhasil! Silakan login.\n" RST);
    else
        printf("\n  " RED "✘ %s\n" RST, resp + 4);
    sleep(2);
}
int do_login(void)
{
    char uname[MAX_NAME], pass[MAX_PASS];
    print_banner();
    printf("  ─── LOGIN ───\n\n");
    printf("  Username : ");
    scanf("%31s", uname);
    printf("  Password : ");
    scanf("%31s", pass);

    char buf[128];
    snprintf(buf, 127, "%s %s", uname, pass);
    send_cmd(MSG_LOGIN, buf);
    char *resp = wait_response();
    if (strncmp(resp, "ERR", 3) == 0)
    {
        printf("\n  " RED "✘ %s\n" RST, resp + 4);
        sleep(2);
        return -1;
    }
    int idx;
    char name[MAX_NAME];
    int gold, lvl, xp, widx;
    sscanf(resp + 3, "%d %s %d %d %d %d", &idx, name, &gold, &lvl, &xp, &widx);
    return idx;
}
int main(void)
{
    mypid = getpid();
    msgid = msgget(MSG_KEY, 0666);
    shmid = shmget(SHM_KEY, sizeof(SharedArena), 0666);
    semid = semget(SEM_KEY, 1, 0666);
    if (msgid < 0 || shmid < 0 || semid < 0)
    {
        printf("\n  " RED "❌ Orion are you there?" RST
               " salah njiir\n\n");
        return 1;
    }
    arena = shmat(shmid, NULL, 0);
    while (1)
    {
        print_banner();
        printf("  1. Register\n");
        printf("  2. Login\n");
        printf("  3. Exit\n\n");
        printf("  > Pilih: ");
        fflush(stdout);

        int c;
        scanf("%d", &c);
        if (c == 1)
        {
            do_register();
        }
        else if (c == 2)
        {
            int pidx = do_login();
            if (pidx >= 0)
                menu_lobby(pidx);
        }
        else
        {
            printf("\n  Keluar dari Eterion. Sampai jumpa!\n\n");
            break;
        }
    }
    shmdt(arena);
    return 0;
}