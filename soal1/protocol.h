
#ifndef COMMON_H
#define COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 5000
#define ADMIN_PASSWORD "lembuBerkah"
#define LOG_FILE "history.log"
typedef enum
{
    MSG_CHAT = 1,
    MSG_SYSTEM = 2,
    MSG_RPC_REQ = 3,
    MSG_RPC_RES = 4,
    MSG_AUTH = 5,
    MSG_EXIT = 6
} MsgType;

typedef enum
{
    RPC_GET_USERS = 1,
    RPC_GET_UPTIME = 2,
    RPC_SHUTDOWN = 3
} RpcCommand;
typedef struct
{
    MsgType type;
    char sender[1000];
    char body[1000];
    int rpc_cmd;
} Packet;
static inline void get_timestamp(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "[%Y-%m-%d %H:%M:%S]", t);
}
#endif