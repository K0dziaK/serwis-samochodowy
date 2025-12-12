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

#define CHANCE_REJECT_INITIAL 2 // 2% klientów odrzuca wstępną wycenę
#define CHANCE_EXTRA_FAULT 20   // 20% szans na dodatkową usterkę
#define CHANCE_REJECT_EXTRA 20  // 20% klientów odrzuca dodatkową naprawę

#define QUEUE_KEY 21370
#define SEM_KEY 21371

#define MSG_TYPE_NEW_CLIENT 1   // Klient -> Obsługa (podejście do kasy)
#define MSG_TYPE_CONSULTATION 2 // Mechanik -> Obsługa (Dodatkowa usterka)
#define MSG_TYPE_BILLING 3      // Mechanik -> Obsługa (Koniec naprawy)

#define MSG_TYPE_MECHANIC_OFFSET 100

typedef struct {
    int id;
    char brand;
    int is_vip; // czy sprawa jest pilna

    char service_name[50];
    int cost;
    int time_est;

    int mechanic_pid;
    int mechanic_id;
    int extra_accepted;
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

typedef struct {
    char name[30];
    int base_cost;
    int base_time;
} Service;

const Service SERVICE_LIST[] = {
    {"Wymiana oleju", 300, 3},
    {"Wymiana klocków", 400, 4},
    {"Naprawa silnika", 2000, 8},
    {"Wymiana opon", 150, 2},
    {"Diagnostyka", 100, 2}
};
const int NUM_SERVICES = 5;

#endif