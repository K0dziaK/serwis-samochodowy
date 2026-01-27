#include "common.h"
#include <sys/ioctl.h>

// Globalny identyfikator semafora (ustawiany w każdym procesie)
int global_sem_id = -1;

// Cennik usług serwisu samochodowego
// Format: {ID, "Nazwa usługi", Koszt bazowy, Czas trwania, Czy krytyczna}
ServiceEntry PRICE_LIST[NUM_SERVICES] = {
    // Usługi podstawowe / eksploatacyjne (niekrytyczne)
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

    // Naprawy mechaniczne (średnie)
    {10, "Wymiana klocków hamulcowych", 400, 4, 1},
    {11, "Wymiana tarcz hamulcowych", 600, 5, 1},
    {12, "Wymiana amortyzatorów", 800, 6, 0},
    {13, "Wymiana świec zapłonowych", 250, 3, 0},
    {14, "Naprawa tłumika", 350, 4, 0},
    {15, "Wymiana akumulatora", 400, 1, 1},
    {16, "Regulacja zbieżności kół", 200, 3, 0},
    {17, "Wymiana paska osprzętu", 300, 4, 0},
    {18, "Naprawa zamka centralnego", 250, 3, 0},
    {19, "Wymiana płynu chłodniczego", 180, 3, 0},

    // Poważne awarie (krytyczne / drogie)
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

// Funkcja obsługi błędów krytycznych - kończy proces z komunikatem
void panic(const char *msg)
{
    perror(msg);
    fprintf(stderr, "[CRIT PID %d] %s: %s\n", getpid(), msg, strerror(errno));
    fflush(stderr);
    exit(EXIT_FAILURE);
}

// Zwraca kolor ANSI odpowiadający roli procesu
static const char *get_role_color(LogRole role)
{
    switch (role)
    {
    case ROLE_MANAGER:
        return COLOR_RED;
    case ROLE_MECHANIC:
        return COLOR_CYAN;
    case ROLE_SERVICE:
        return COLOR_GREEN;
    case ROLE_CASHIER:
        return COLOR_YELLOW;
    case ROLE_CLIENT:
        return COLOR_BLUE;
    case ROLE_GENERATOR:
        return COLOR_MAGENTA;
    case ROLE_SYSTEM:
        return COLOR_GRAY;
    default:
        return COLOR_WHITE;
    }
}

// Zwraca nazwę roli do wyświetlenia w logach
static const char *get_role_name(LogRole role)
{
    switch (role)
    {
    case ROLE_MANAGER:
        return "MANAGER  ";
    case ROLE_MECHANIC:
        return "MECHANIK ";
    case ROLE_SERVICE:
        return "OBSŁUGA  ";
    case ROLE_CASHIER:
        return "KASJER   ";
    case ROLE_CLIENT:
        return "KLIENT   ";
    case ROLE_GENERATOR:
        return "GENERATOR";
    case ROLE_SYSTEM:
        return "SYSTEM   ";
    default:
        return "???      ";
    }
}

// Loguje akcję do pliku raport.txt i na konsolę
void log_action(const char *format, ...)
{
    if (global_sem_id == -1)
        return;
    
    // Blokowanie semafora logowania (synchronizacja)
    sem_lock(global_sem_id, SEM_LOG);
    FILE *f = fopen("raport.txt", "a");
    if (f)
    {
        va_list args;
        time_t now;
        time(&now);
        char buff[20];
        strftime(buff, 20, "%H:%M:%S", localtime(&now));
        
        // Zapis do pliku
        fprintf(f, "[%s] ", buff);
        printf(COLOR_GRAY "[%s]" COLOR_RESET " ", buff);
        va_start(args, format);
        vfprintf(f, format, args);
        va_end(args);
        
        // Wyświetlanie na konsoli
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fprintf(f, "\n");
        printf("\n");
        fflush(stdout);
        fclose(f);
    }
    sem_unlock(global_sem_id, SEM_LOG);
}

// Loguje akcję z kolorowaniem według roli procesu
void log_color(LogRole role, const char *format, ...)
{
    if (global_sem_id == -1)
        return;
    
    // Blokowanie semafora logowania
    sem_lock(global_sem_id, SEM_LOG);
    FILE *f = fopen("raport.txt", "a");
    if (f)
    {
        va_list args;
        time_t now;
        time(&now);
        char buff[20];
        strftime(buff, 20, "%H:%M:%S", localtime(&now));

        const char *color = get_role_color(role);
        const char *role_name = get_role_name(role);

        // Zapis do pliku (bez kolorów)
        fprintf(f, "[%s] [%s] ", buff, role_name);
        va_start(args, format);
        vfprintf(f, format, args);
        va_end(args);
        fprintf(f, "\n");

        // Wyświetlanie na konsoli (z kolorami)
        printf(COLOR_GRAY "[%s]" COLOR_RESET " %s[%s]" COLOR_RESET " ", buff, color, role_name);
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
        fflush(stdout);

        fclose(f);
    }
    sem_unlock(global_sem_id, SEM_LOG);
}

// ============= BEZPIECZNE WRAPPERY WYWOŁAŃ SYSTEMOWYCH IPC =============

// Bezpieczne dołączenie pamięci dzielonej
void *safe_shmat(int shmid, const void *shmaddr, int shmflg)
{
    void *ptr = shmat(shmid, shmaddr, shmflg);
    if (ptr == (void *)-1)
        panic("shmat failed");
    return ptr;
}

// Bezpieczne pobranie kolejki komunikatów
int safe_msgget(key_t key, int msgflg)
{
    int id = msgget(key, msgflg);
    if (id == -1)
        panic("msgget failed");
    return id;
}

// Bezpieczne pobranie pamięci dzielonej
int safe_shmget(key_t key, size_t size, int shmflg)
{
    int id = shmget(key, size, shmflg);
    if (id == -1)
        panic("shmget failed");
    return id;
}

// Bezpieczne pobranie semafora
int safe_semget(key_t key, int nsems, int semflg)
{
    int id = semget(key, nsems, semflg);
    if (id == -1)
        panic("semget failed");
    return id;
}

// Bezpieczne tworzenie procesu potomnego
pid_t safe_fork(void)
{
    pid_t p = fork();
    if (p == -1)
        panic("fork failed");
    return p;
}

// Bezpieczna operacja na semaforze
// Automatycznie ponawia operację gdy zostanie przerwana przez sygnał (EINTR)
// Przy usunięciu semafora (EIDRM/EINVAL) proces kończy działanie
void safe_semop(int semid, struct sembuf *sops, size_t nsops)
{
    // Pętla ponawiania - kluczowa dla poprawnego działania po wznowieniu procesu
    while (1)
    {
        if (semop(semid, sops, nsops) == 0)
            return; // Operacja zakończona sukcesem
        
        if (errno == EINTR)
            continue; // Przerwanie przez sygnał - ponowienie operacji
        
        // Semafor usunięty - proces kończy pracę (symulacja zamykana)
        if (errno == EIDRM || errno == EINVAL)
            exit(0);
        
        panic("semop failed");
    }
}

// Bezpieczne wysłanie komunikatu
// Automatycznie ponawia operację gdy zostanie przerwana przez sygnał (EINTR)
void safe_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
    // Pętla ponawiania - kluczowa dla poprawnego działania po wznowieniu procesu
    while (1)
    {
        if (msgsnd(msqid, msgp, msgsz, msgflg) == 0)
            return; // Wysłanie zakończone sukcesem
        
        if (errno == EINTR)
            continue; // Przerwanie przez sygnał - ponowienie operacji
        
        // Kolejka usunięta - proces kończy pracę (symulacja zamykana)
        if (errno == EIDRM || errno == EINVAL)
            exit(0);
        
        panic("msgsnd failed");
    }
}

// Nieblokujące odbieranie komunikatu (zwraca -1 gdy brak wiadomości)
// Przy braku wiadomości lub przerwaniu przez sygnał zwraca -1 bez błędu
ssize_t safe_msgrcv_nowait(int msqid, void *msgp, size_t msgsz, long msgtyp)
{
    ssize_t ret = msgrcv(msqid, msgp, msgsz, msgtyp, IPC_NOWAIT);
    if (ret == -1)
    {
        // Brak wiadomości w kolejce - normalna sytuacja
        if (errno == ENOMSG)
            return -1;
        // Przerwanie przez sygnał - traktowane jak brak wiadomości
        if (errno == EINTR)
            return -1;
        // Kolejka usunięta - proces kończy pracę (symulacja zamykana)
        if (errno == EIDRM || errno == EINVAL)
            exit(0);
        panic("msgrcv nowait failed");
    }
    return ret;
}

// Blokujące odbieranie komunikatu (czeka na wiadomość)
// Automatycznie ponawia oczekiwanie gdy zostanie przerwane przez sygnał (EINTR)
// Dzięki temu po wznowieniu procesu (SIGCONT) operacja jest kontynuowana
ssize_t safe_msgrcv_wait(int msqid, void *msgp, size_t msgsz, long msgtyp)
{
    // Pętla ponawiania - kluczowa dla poprawnego działania po wznowieniu procesu
    while (1)
    {
        ssize_t ret = msgrcv(msqid, msgp, msgsz, msgtyp, 0);
        if (ret >= 0)
            return ret; // Odebrano wiadomość
        
        if (errno == EINTR)
            continue; // Przerwanie przez sygnał - ponowienie oczekiwania
        
        // Kolejka usunięta - proces kończy pracę (symulacja zamykana)
        if (errno == EIDRM || errno == EINVAL)
            exit(0);
        
        panic("msgrcv wait failed");
    }
}

// ============= FUNKCJE POMOCNICZE SEMAFORÓW =============

// Blokowanie semafora (operacja P / wait)
void sem_lock(int semid, int sem_num)
{
    struct sembuf sb = {sem_num, -1, 0};
    safe_semop(semid, &sb, 1);
}

// Zwalnianie semafora (operacja V / signal)
void sem_unlock(int semid, int sem_num)
{
    struct sembuf sb = {sem_num, 1, 0};
    safe_semop(semid, &sb, 1);
}

// Opóźnienie z możliwością wyłączenia (do testów)
void custom_usleep(unsigned int usec)
{
#ifdef TESTING_NO_SLEEP
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

    // Przeliczanie sekund rzeczywistych na minuty symulacji
    long total_sim_minutes = (long)(seconds_elapsed * SIM_MINUTES_PER_REAL_SEC);

    // Symulacja startuje o godzinie 7:00
    int start_hour = 7;
    long total_minutes_from_start = start_hour * 60 + total_sim_minutes;

    // Obliczanie dnia, godziny i minuty
    st.day = total_minutes_from_start / (24 * 60);
    int minutes_in_day = total_minutes_from_start % (24 * 60);
    st.hour = minutes_in_day / 60;
    st.minute = minutes_in_day % 60;

    return st;
}