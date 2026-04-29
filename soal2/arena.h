#ifndef ARENA_H
#define ARENA_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <time.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#define SHM_KEY 0x00001234
#define MSG_KEY 0x00005678
#define SEM_KEY 0x00009012

#define MAX_PLAYERS 16
#define MAX_NAME 32
#define MAX_PASS 32
#define MAX_HISTORY 20
#define BASE_DMG 10
#define BASE_HP 100
#define MATCHMAKE_TIMEOUT 35
#define WEAPON_COUNT 4
typedef struct
{
    char name[24];
    int bonus_dmg;
    int price;
} Weapon;
static const Weapon WEAPONS[WEAPON_COUNT] = {
    {"Wood Sword", 5, 100},
    {"Iron Sword", 15, 250},
    {"Steel Sword", 30, 500},
    {"Dragon Blade", 60, 1200},
};
typedef struct
{
    char opponent[MAX_NAME];
    char result[8];
    int xp_gained;
    char timestamp[32];
} HistoryEntry;
typedef struct
{
    char username[MAX_NAME];
    char password[MAX_PASS];
    int gold;
    int level;
    int xp;
    int weapon_idx;
    int online;
    int in_battle;
    int battle_slot;
    HistoryEntry history[MAX_HISTORY];
    int history_count;
} Player;
#define MAX_BATTLES 4
typedef struct
{
    int active;
    int p1_idx;
    int p2_idx;
    int p1_hp;
    int p2_hp;
    int p1_max_hp;
    int p2_max_hp;
    int p1_dmg;
    int p2_dmg;
    int winner;
    long waiting_pid;
    int react_target;
    time_t react_time;
    int react_answered;
    char react_char;
    int react_round;
    int p1_ult_used; 
    int p2_ult_used;
} Battle;
#define MSG_ULTIMATE   9
typedef struct
{
    Player players[MAX_PLAYERS];
    int player_count;
    Battle battles[MAX_BATTLES];
} SharedArena;

#define MSG_REGISTER 1
#define MSG_LOGIN 2
#define MSG_LOGOUT 3
#define MSG_BATTLE_REQ 4
#define MSG_ATTACK 5
#define MSG_REACT_OK 6
#define MSG_BUY 7
#define MSG_BOT_REQ 8
#define MSG_RESPONSE 100

typedef struct
{
    long mtype;
    char data[256];
} ArenaMsg;

static inline void sem_lock(int semid)
{
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
}
static inline void sem_unlock(int semid)
{
    struct sembuf sb = {0, 1, 0};
    semop(semid, &sb, 1);
}

static struct termios orig_termios;
static inline void set_raw_mode(void)
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &orig_termios);
    t = orig_termios;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}
static inline void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

#endif