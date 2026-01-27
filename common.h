#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE

// Tryb testowy - odkomentowanie wyłącza usleep (przydatne do debugowania)
// #define TESTING_NO_SLEEP

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

// ============= KOLORY KONSOLI (ANSI) =============
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN "\033[1;36m"
#define COLOR_WHITE "\033[1;37m"
#define COLOR_ORANGE "\033[38;5;208m"
#define COLOR_GRAY "\033[1;90m"

// Role procesów (do kolorowania logów)
typedef enum
{
    ROLE_MANAGER,
    ROLE_MECHANIC,
    ROLE_SERVICE,
    ROLE_CASHIER,
    ROLE_CLIENT,
    ROLE_GENERATOR,
    ROLE_SYSTEM
} LogRole;

// ============= KONFIGURACJA SERWISU =============
#define NUM_MECHANICS 8                // Liczba mechaników
#define NUM_SERVICE_STAFF 3            // Liczba stanowisk obsługi
#define OPEN_HOUR 8                    // Godzina otwarcia
#define CLOSE_HOUR 18                  // Godzina zamknięcia
#define SIM_MINUTES_PER_REAL_SEC 30    // Tempo symulacji (minuty symulacji / sekunda rzeczywista)
#define K1 3                           // Próg kolejki do uruchomienia stanowiska 2
#define K2 5                           // Próg kolejki do uruchomienia stanowiska 3
#define T1 2                           // Maksymalny czas oczekiwania na otwarcie (godziny)
#define CHANCE_RESIGNATION 2           // Szansa (%) na rezygnację klienta z oferty
#define CHANCE_EXTRA_REPAIR_REFUSAL 20 // Szansa (%) na odmowę dodatkowej naprawy
#define CHANCE_EXTRA_REPAIR 20         // Szansa (%) na wykrycie dodatkowej usterki

// ============= CENNIK USŁUG =============
#define NUM_SERVICES 30

typedef struct
{
    int id;
    const char *name;
    int base_cost;     // Koszt bazowy (PLN)
    int base_duration; // Czas bazowy (jednostki symulacji)
    int is_critical;   // 1 = naprawa krytyczna, 0 = zwykła
} ServiceEntry;

extern ServiceEntry PRICE_LIST[NUM_SERVICES];

// ============= KLUCZE IPC =============
#define FTOK_PATH "main.c"
#define PROJ_ID_MSG 'A'
#define PROJ_ID_SHM 'B'
#define PROJ_ID_SEM 'C'

// ============= TYPY KOMUNIKATÓW =============
// Komunikaty bez adresowania (stały mtype)
#define MSG_CLIENT_TO_SERVICE 1
#define MSG_MECHANIC_TO_SERVICE 2
#define MSG_SERVICE_TO_CASHIER 3

// Komunikaty z adresowaniem PID (mtype = BASE + pid)
#define MSG_BASE_TO_CLIENT 1000000L       // Wiadomość do klienta
#define MSG_BASE_CLIENT_DECISION 2000000L // Decyzja klienta
#define MSG_BASE_TO_MECHANIC 3000000L     // Wiadomość do mechanika
#define MSG_BASE_CLIENT_PAYMENT 4000000L  // Płatność klienta

// ============= STRUKTURA KOMUNIKATU =============
typedef struct
{
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

// ============= PAMIĘĆ DZIELONA =============
typedef struct
{
    time_t start_time;
    int simulation_running;
    int clients_in_queue;
    pid_t mechanics_pids[NUM_MECHANICS];
    int mechanic_status[NUM_MECHANICS]; // 0=wolny, 1=zajęty, 2=zamknięty
    pid_t service_pids[NUM_SERVICE_STAFF];
} shared_data;

// ============= SEMAFORY =============
#define SEM_LOG 0            // Synchronizacja logowania
#define SEM_QUEUE 1          // Limit miejsc w kolejce
#define SEM_PROC_LIMIT 2     // Limit procesów klientów
#define SEM_MECHANIC_ASSIGN 3 // Synchronizacja przypisywania zleceń do mechaników
#define SEM_NUM 4

// ============= SYGNAŁY STERUJĄCE =============
#define SIG_CLOSE_STATION SIGUSR1  // Zamknięcie stanowiska mechanika
#define SIG_SPEED_UP SIGUSR2       // Przyspieszenie pracy mechanika
#define SIG_RESTORE_SPEED SIGRTMIN // Przywrócenie normalnego tempa
#define SIG_FIRE SIGTERM           // Ewakuacja (pożar)

// Globalny identyfikator semafora
extern int global_sem_id;

// ============= PASEK STATUSU =============
#define ANSI_SAVE_CURSOR "\033[s"
#define ANSI_RESTORE_CURSOR "\033[u"
#define ANSI_MOVE_TO_BOTTOM "\033[999B"
#define ANSI_CLEAR_LINE "\033[2K"
#define ANSI_MOVE_UP "\033[1A"
#define ANSI_SCROLL_REGION "\033[1;%dr"
#define ANSI_RESET_SCROLL "\033[r"
#define ANSI_MOVE_TO "\033[%d;1H"

// ============= STRUKTURA CZASU SYMULACJI =============
typedef struct
{
    int day;
    int hour;
    int minute;
} sim_time;

// Obliczanie czasu symulacji
sim_time get_simulation_time(time_t start_time);

// ============= FUNKCJE POMOCNICZE =============

// Obsługa błędów
void panic(const char *msg);

// Logowanie
void log_action(const char *format, ...);
void log_color(LogRole role, const char *format, ...);

// Bezpieczne wrappery IPC
void *safe_shmat(int shmid, const void *shmaddr, int shmflg);
int safe_msgget(key_t key, int msgflg);
int safe_shmget(key_t key, size_t size, int shmflg);
int safe_semget(key_t key, int nsems, int semflg);
void safe_semop(int semid, struct sembuf *sops, size_t nsops);
pid_t safe_fork(void);
ssize_t safe_msgrcv_nowait(int msqid, void *msgp, size_t msgsz, long msgtyp);
ssize_t safe_msgrcv_wait(int msqid, void *msgp, size_t msgsz, long msgtyp);
void safe_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);

// Operacje semaforowe
void sem_lock(int semid, int sem_num);
void sem_unlock(int semid, int sem_num);

// Opóźnienie (z możliwością wyłączenia do testów)
void custom_usleep(unsigned int usec);

#endif