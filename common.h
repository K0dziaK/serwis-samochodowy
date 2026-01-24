#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE 
// #define TESTING_NO_SLEEP // Odkomentowanie tej linii wyłącza wszelkie usleep w programie (do testów)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

// --- KOLORY KONSOLI (ANSI) ---
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"
#define COLOR_ORANGE  "\033[38;5;208m"
#define COLOR_GRAY    "\033[1;90m"

// Role dla kolorowania
typedef enum {
    ROLE_MANAGER,
    ROLE_MECHANIC,
    ROLE_SERVICE,
    ROLE_CASHIER,
    ROLE_CLIENT,
    ROLE_GENERATOR,
    ROLE_SYSTEM
} LogRole;

// --- KONFIGURACJA ---
#define NUM_MECHANICS               8   // Liczba mechaników w serwisie
#define NUM_SERVICE_STAFF           3   // Liczba stanowisk obsługi klienta
#define OPEN_HOUR                   8   // Godzina otwarcia serwisu
#define CLOSE_HOUR                  18  // Godzina zamknięcia serwisu
#define SIM_MINUTES_PER_REAL_SEC    30  // Liczba minut symulacji na sekundę rzeczywistą
#define K1                          3   // Próg klientów w kolejce do uruchomienia dodatkowej obsługi
#define K2                          5   // Próg klientów w kolejce do uruchomienia drugiej dodatkowej obsługi
#define T1                          1   // Próg czasu (godziny) do otwarcia, przy którym klient czeka
#define CHANCE_RESIGNATION          2   // Szansa (%) na rezygnację klienta z naprawy przy dodatkowej usterce
#define CHANCE_EXTRA_REPAIR_REFUSAL 20  // Szansa (%) na odmowę dodatkowej naprawy przez klienta
#define CHANCE_EXTRA_REPAIR         20  // Szansa (%) na wykrycie dodatkowej usterki przez mechanika

// --- CENNIK ---
#define NUM_SERVICES               30   // Liczba różnych usług w cenniku

typedef struct {
    int id;
    const char* name;
    int base_cost;      // Koszt bazowy
    int base_duration;  // Czas w jednostkach symulacji
    int is_critical;    // 1 - tak, 0 - nie
} ServiceEntry;

// Deklaracja cennika
extern ServiceEntry PRICE_LIST[NUM_SERVICES];

// --- IPC ---
#define FTOK_PATH "main.c"
#define PROJ_ID_MSG 'A'
#define PROJ_ID_SHM 'B'
#define PROJ_ID_SEM 'C'

// --- TYPY KOMUNIKATÓW ---
// Stałe typy (bez adresowania PID)
#define MSG_CLIENT_TO_SERVICE   1
#define MSG_MECHANIC_TO_SERVICE 2
#define MSG_SERVICE_TO_CASHIER  3

// Bazowe typy dla adresowania z PID (mtype = BASE + pid)
#define MSG_BASE_TO_CLIENT          1000000L       // Wiadomość do klienta (od obsługi/kasjera)
#define MSG_BASE_CLIENT_DECISION    2000000L       // Decyzja klienta (do obsługi/mechanika)
#define MSG_BASE_TO_MECHANIC        3000000L       // Wiadomość do mechanika (od obsługi)
#define MSG_BASE_CLIENT_PAYMENT     4000000L       // Płatność od klienta (do kasjera)

// Struktura komunikatu
typedef struct {
    long mtype;
    pid_t client_pid;
    pid_t sender_pid;
    int mechanic_id;
    char brand;
    int service_id;
    int cost;
    int duration;
    int is_extra_repair;
    int decision;
} msg_buf;

// Struktura danych współdzielonych
typedef struct {
    time_t start_time;
    int simulation_running;
    int clients_in_queue;
    pid_t mechanics_pids[NUM_MECHANICS];
    int mechanic_status[NUM_MECHANICS];   
    pid_t service_pids[NUM_SERVICE_STAFF]; 
} shared_data;

// Semafory
#define SEM_LOG 0
#define SEM_QUEUE 1
#define SEM_PROC_LIMIT 2
#define SEM_NUM 3

// Sygnały wewnętrzne
#define SIG_CLOSE_STATION SIGUSR1
#define SIG_SPEED_UP      SIGUSR2
#define SIG_RESTORE_SPEED SIGRTMIN
#define SIG_FIRE          SIGTERM

// Globalne ID semafora (ustawiane w każdym procesie)
extern int global_sem_id;

// Struktura przechowująca czas symulacji
typedef struct {
    int day;       // Dzień symulacji (0-based)
    int hour;      // Godzina (0-23)
    int minute;    // Minuta (0-59)
} sim_time;

// Funkcja obliczająca czas symulacji na podstawie start_time
sim_time get_simulation_time(time_t start_time);

// Funkcje bezpiecznych wrapperów IPC
void panic(const char* msg);
void log_action(const char* format, ...);
void log_color(LogRole role, const char* format, ...);
void* safe_shmat(int shmid, const void *shmaddr, int shmflg);
int safe_msgget(key_t key, int msgflg);
int safe_shmget(key_t key, size_t size, int shmflg);
int safe_semget(key_t key, int nsems, int semflg);
void safe_semop(int semid, struct sembuf *sops, size_t nsops);
pid_t safe_fork(void);
ssize_t safe_msgrcv_nowait(int msqid, void *msgp, size_t msgsz, long msgtyp);
ssize_t safe_msgrcv_wait(int msqid, void *msgp, size_t msgsz, long msgtyp);
void safe_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);

// Semaforowe funkcje pomocnicze
void sem_lock(int semid, int sem_num);
void sem_unlock(int semid, int sem_num);

// Dla łatwego testowania programu bez sleepa
void custom_usleep(unsigned int usec);

#endif