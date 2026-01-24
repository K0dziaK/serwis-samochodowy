#include "common.h"

int global_sem_id = -1;

// Format: {ID, "Nazwa", Koszt, Czas, CzyKrytyczna}
ServiceEntry PRICE_LIST[NUM_SERVICES] = {
    // Podstawowe / Eksploatacja (Niekrytyczne)
    {0, "Wymiana oleju i filtrów", 300, 2, 0},
    {1, "Wymiana opon (sezonowa)", 150, 2, 0},
    {2, "Wymiana żarówki", 50, 1, 0},
    {3, "Wymiana wycieraczek", 80, 1, 0},
    {4, "Przegląd klimatyzacji", 200, 3, 0},
    {5, "Mycie i woskowanie", 120, 2, 0},
    {6, "Wymiana płynu spryskiwaczy", 30, 1, 0},
    {7, "Diagnostyka komputerowa", 150, 2, 0},
    {8, "Wymiana filtra powietrza", 60, 1, 0},
    {9, "Wymiana filtra kabinowego", 70, 1, 0},

    // Naprawy mechaniczne (Średnie)
    {10, "Wymiana klocków hamulcowych", 400, 4, 1}, // Hamulce to krytyczne
    {11, "Wymiana tarcz hamulcowych", 600, 5, 1},
    {12, "Wymiana amortyzatorów", 800, 6, 0},
    {13, "Wymiana świec zapłonowych", 250, 3, 0},
    {14, "Naprawa tłumika", 350, 4, 0},
    {15, "Wymiana akumulatora", 400, 1, 1}, // Bez prądu nie pojedzie
    {16, "Regulacja zbieżności kół", 200, 3, 0},
    {17, "Wymiana paska osprzętu", 300, 4, 0},
    {18, "Naprawa zamka centralnego", 250, 3, 0},
    {19, "Wymiana płynu chłodniczego", 180, 3, 0},

    // Poważne awarie (Krytyczne / Drogie)
    {20, "Awaria układu kierowniczego", 1500, 10, 1},
    {21, "Zerwany pasek rozrządu", 2500, 15, 1},
    {22, "Wyciek paliwa", 600, 5, 1},
    {23, "Awaria pompy hamulcowej", 900, 6, 1},
    {24, "Przegrzewanie silnika (UPG)", 3000, 20, 1},
    {25, "Awaria skrzyni biegów", 4000, 18, 1},
    {26, "Wymiana sprzęgła", 1800, 12, 1},
    {27, "Naprawa alternatora", 500, 5, 1},
    {28, "Naprawa rozrusznika", 450, 5, 1},
    {29, "Pęknięta sprężyna zawieszenia", 400, 4, 1}};

// Funkcja pomocnicza do obsługi błędów krytycznych
void panic(const char *msg)
{
    perror(msg);  // Wymagane przez specyfikację projektu
    fprintf(stderr, "[CRIT PID %d] %s: %s\n", getpid(), msg, strerror(errno));
    fflush(stderr);
    exit(EXIT_FAILURE);
}

// Pomocnicza funkcja do uzyskania koloru na podstawie roli
static const char* get_role_color(LogRole role)
{
    switch (role) {
        case ROLE_MANAGER:   return COLOR_RED;
        case ROLE_MECHANIC:  return COLOR_CYAN;
        case ROLE_SERVICE:   return COLOR_GREEN;
        case ROLE_CASHIER:   return COLOR_YELLOW;
        case ROLE_CLIENT:    return COLOR_BLUE;
        case ROLE_GENERATOR: return COLOR_MAGENTA;
        case ROLE_SYSTEM:    return COLOR_GRAY;
        default:             return COLOR_WHITE;
    }
}

// Pomocnicza funkcja do uzyskania nazwy roli
static const char* get_role_name(LogRole role)
{
    switch (role) {
        case ROLE_MANAGER:   return "MANAGER  ";
        case ROLE_MECHANIC:  return "MECHANIK ";
        case ROLE_SERVICE:   return "OBSŁUGA  ";
        case ROLE_CASHIER:   return "KASJER   ";
        case ROLE_CLIENT:    return "KLIENT   ";
        case ROLE_GENERATOR: return "GENERATOR";
        case ROLE_SYSTEM:    return "SYSTEM   ";
        default:             return "???      ";
    }
}

// Funkcja logująca akcje do pliku raport.txt
void log_action(const char *format, ...)
{
    if (global_sem_id == -1)
        return;
    sem_lock(global_sem_id, SEM_LOG);
    FILE *f = fopen("raport.txt", "a");
    if (f)
    {
        va_list args;
        time_t now;
        time(&now);
        char buff[20];
        strftime(buff, 20, "%H:%M:%S", localtime(&now));
        fprintf(f, "[%s] ", buff);
        printf(COLOR_GRAY "[%s]" COLOR_RESET " ", buff);
        va_start(args, format);
        vfprintf(f, format, args);
        va_end(args);
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fprintf(f, "\n");
        printf("\n");
        fclose(f);
    }
    sem_unlock(global_sem_id, SEM_LOG);
}

// Funkcja logująca z kolorowaniem na podstawie roli
void log_color(LogRole role, const char *format, ...)
{
    if (global_sem_id == -1)
        return;
    sem_lock(global_sem_id, SEM_LOG);
    FILE *f = fopen("raport.txt", "a");
    if (f)
    {
        va_list args;
        time_t now;
        time(&now);
        char buff[20];
        strftime(buff, 20, "%H:%M:%S", localtime(&now));
        
        const char* color = get_role_color(role);
        const char* role_name = get_role_name(role);
        
        // Do pliku - bez kolorów
        fprintf(f, "[%s] [%s] ", buff, role_name);
        va_start(args, format);
        vfprintf(f, format, args);
        va_end(args);
        fprintf(f, "\n");
        
        // Do konsoli - z kolorami
        printf(COLOR_GRAY "[%s]" COLOR_RESET " %s[%s]" COLOR_RESET " ", buff, color, role_name);
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
        
        fclose(f);
    }
    sem_unlock(global_sem_id, SEM_LOG);
}

// Bezpieczne opakowania systemowych wywołań IPC
void *safe_shmat(int shmid, const void *shmaddr, int shmflg)
{
    void *ptr = shmat(shmid, shmaddr, shmflg);
    if (ptr == (void *)-1)
        panic("shmat failed");
    return ptr;
}
int safe_msgget(key_t key, int msgflg)
{
    int id = msgget(key, msgflg);
    if (id == -1)
        panic("msgget failed");
    return id;
}
int safe_shmget(key_t key, size_t size, int shmflg)
{
    int id = shmget(key, size, shmflg);
    if (id == -1)
        panic("shmget failed");
    return id;
}
int safe_semget(key_t key, int nsems, int semflg)
{
    int id = semget(key, nsems, semflg);
    if (id == -1)
        panic("semget failed");
    return id;
}
pid_t safe_fork(void)
{
    pid_t p = fork();
    if (p == -1)
        panic("fork failed");
    return p;
}
void safe_semop(int semid, struct sembuf *sops, size_t nsops)
{
    if (semop(semid, sops, nsops) == -1)
    {
        if (errno == EINTR)
            return;
        if (errno == EIDRM || errno == EINVAL)
            exit(0);
        panic("semop failed");
    }
}
void safe_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
    if (msgsnd(msqid, msgp, msgsz, msgflg) == -1)
    {
        if (errno == EINTR)
            return;
        if (errno == EIDRM || errno == EINVAL)
            exit(0);
        panic("msgsnd failed");
    }
}
ssize_t safe_msgrcv_nowait(int msqid, void *msgp, size_t msgsz, long msgtyp)
{
    ssize_t ret = msgrcv(msqid, msgp, msgsz, msgtyp, IPC_NOWAIT);
    if (ret == -1)
    {
        if (errno == ENOMSG)
            return -1;
        if (errno == EINTR)
            return -1;
        if (errno == EIDRM || errno == EINVAL)
            exit(0);
        panic("msgrcv nowait failed");
    }
    return ret;
}
ssize_t safe_msgrcv_wait(int msqid, void *msgp, size_t msgsz, long msgtyp)
{
    ssize_t ret = msgrcv(msqid, msgp, msgsz, msgtyp, 0);
    if (ret == -1)
    {
        if (errno == EINTR)
            return -1;
        if (errno == EIDRM || errno == EINVAL)
            exit(0);
        panic("msgrcv wait failed");
    }
    return ret;
}

// Semafory pomocnicze
void sem_lock(int semid, int sem_num)
{
    struct sembuf sb = {sem_num, -1, 0};
    safe_semop(semid, &sb, 1);
}
void sem_unlock(int semid, int sem_num)
{
    struct sembuf sb = {sem_num, 1, 0};
    safe_semop(semid, &sb, 1);
}

// Dla łatwego testowania programu bez sleepa
void custom_usleep(unsigned int usec)
{
#ifdef TESTING_NO_SLEEP
    // Nie śpimy w trybie testowym
    (void)usec;
#else
    usleep(usec);
#endif
}

// Oblicza czas symulacji na podstawie rzeczywistego upływu czasu
sim_time get_simulation_time(time_t start_time)
{
    sim_time st;
    time_t now = time(NULL);
    double seconds_elapsed = difftime(now, start_time);
    
    // Przeliczenie na minuty symulacji
    long total_sim_minutes = (long)(seconds_elapsed * SIM_MINUTES_PER_REAL_SEC);
    
    // Godzina startowa symulacji to 7:00
    int start_hour = 7;
    long total_minutes_from_start = start_hour * 60 + total_sim_minutes;
    
    // Obliczenie dnia, godziny i minuty
    st.day = total_minutes_from_start / (24 * 60);
    int minutes_in_day = total_minutes_from_start % (24 * 60);
    st.hour = minutes_in_day / 60;
    st.minute = minutes_in_day % 60;
    
    return st;
}