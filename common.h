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
#include <sys/shm.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define NUM_MECHANICS 8
#define VIP_CUSTOMER_CHANCE 2

#define SIG_CLOSE_STATION   SIGUSR1     // Zamknij stanowisko
#define SIG_SPEED_UP        SIGUSR2     // Przyśpiesz pracę
#define SIG_NORMAL_SPEED    SIGRTMIN    // Przywróć normalny czas
#define SIG_FIRE            SIGQUIT     // Pożar

// Godziny pracy
#define TP 8                    // Godzina otwarcia
#define TK 18                   // Godzina zamknięcia
#define T1 2                    // Czas oczekiwania przed otwarciem

#define CHANCE_REJECT_INITIAL 2 // 2% klientów odrzuca wstępną wycenę
#define CHANCE_EXTRA_FAULT 20   // 20% szans na dodatkową usterkę
#define CHANCE_REJECT_EXTRA 20  // 20% klientów odrzuca dodatkową naprawę

#define QUEUE_KEY 21370
#define SEM_KEY 21371
#define SHM_TIME_KEY 21372
#define SHM_PIDS_KEY 21373

#define MSG_TYPE_NEW_CLIENT 1   // Klient -> Obsługa (podejście do kasy)
#define MSG_TYPE_CONSULTATION 2 // Mechanik -> Obsługa (Dodatkowa usterka)
#define MSG_TYPE_BILLING 3      // Mechanik -> Obsługa (Koniec naprawy)

#define MSG_TYPE_MECHANIC_OFFSET 100

// --- Struktury danych ---
typedef struct {
    int id;
    char brand;
    int is_vip; // czy sprawa jest pilna

    char service_name[50];
    int cost;
    int time_est;
    int is_critical_fault;

    int mechanic_pid;
    int mechanic_id;
    int extra_accepted;
} CarData;

typedef struct {
    long mtype;
    CarData data;
} CarMessage;

typedef struct {
    char name[30];
    int base_cost;
    int base_time;
    int is_critical;
} Service;

typedef struct {
    int current_hour;
} SharedTime;

typedef struct {
    pid_t mechanic_pids[NUM_MECHANICS];
} SharedData;

extern int msg_queue_id;
extern int sem_id;
extern const char ALLOWED_BRANDS[];
extern const int NUM_ALLOWED_BRANDS;
extern const Service SERVICE_LIST[];
extern const int NUM_SERVICES;
extern int shm_pids_id;

#endif