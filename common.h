#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define NUM_MECHANICS 8
#define MIN_SERVICE_DESKS 1
#define MAX_SERVICE_DESKS 3

#define QUEUE_KEY 21370

#define MSG_TYPE_NEW_CLIENT 1
#define MSG_TYPE_TO_REPAIR 2

typedef struct {
    int id;
    char brand;
    int is_vip; // czy sprawa jest pilna
} CarData;

typedef struct {
    long mtype;
    CarData data;
} CarMessage;

const char ALLOWED_BRANDS[] = {'A','E','I','O','U','Y'};
const int NUM_ALLOWED_BRANDS = 6;

#endif