#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define NUM_MECHANICS 8
#define VIP_CUSTOMER_CHANCE 2

#define QUEUE_KEY 21370
#define SEM_KEY 21371

#define MSG_TYPE_NEW_CLIENT 1
#define MSG_TYPE_MECHANIC_OFFSET 100

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

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

#endif