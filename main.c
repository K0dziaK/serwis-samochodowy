#include "common.h"

// Identyfikatory zasobów IPC
int msg_id, shm_id, sem_id;

// Tablica PID-ów procesów potomnych
pid_t pids[100];
int pid_count = 0;

// Wskaźnik do pamięci dzielonej (dla handlerów sygnałów)
shared_data *global_shm = NULL;

// Ścieżki do programów wykonywalnych
#define MECHANIC_EXEC_PATH "./mechanic"
#define SERVICE_EXEC_PATH "./service"
#define CASHIER_EXEC_PATH "./cashier"
#define MANAGER_EXEC_PATH "./manager"
#define GENERATOR_EXEC_PATH "./generator"

// Funkcja sprzątająca zasoby przy zakończeniu symulacji
// Wysyła SIGTERM do wszystkich procesów potomnych i zwalnia zasoby IPC
void cleanup()
{
    printf("\n" COLOR_YELLOW "[MAIN]" COLOR_RESET " Zamykanie symulacji...\n");

    // Wysyłanie SIGTERM do wszystkich procesów potomnych
    for (int i = 0; i < pid_count; i++)
    {
        if (pids[i] > 0)
            kill(pids[i], SIGTERM);
    }

    // Chwila dla menedżera na zamknięcie swoich procesów
    usleep(500000);

    // Usuwanie zasobów IPC (kolejka komunikatów, pamięć dzielona, semafory)
    msgctl(msg_id, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    printf(COLOR_GREEN "[MAIN]" COLOR_RESET " Zasoby zwolnione. Koniec.\n");
}

// Resetowanie handlerów sygnałów dla procesów potomnych
// Procesy potomne ignorują SIGTSTP (są zatrzymywane przez SIGSTOP z procesu głównego)
void reset_child_signals()
{
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
}

// Handler SIGINT (Ctrl+C) - zamknięcie symulacji
// Wywoływany gdy użytkownik naciśnie Ctrl+C
void sigint_handler(int sig)
{
    (void)sig;
    cleanup();
    exit(0);
}

// Deklaracja funkcji do ustawienia handlerów
void setup_signal_handlers(void);

// Handler SIGTSTP (Ctrl+Z) - wstrzymanie symulacji
// Zatrzymuje wszystkie procesy potomne za pomocą SIGSTOP,
// a następnie zatrzymuje siebie
void sigtstp_handler(int sig)
{
    (void)sig;
    printf("\n\n" COLOR_ORANGE ">>>" COLOR_RESET " [MAIN] WSTRZYMANIE (CTRL+Z) " COLOR_ORANGE "<<<" COLOR_RESET "\n");
    
    // Zatrzymanie procesów uruchomionych przez main
    for (int i = 0; i < pid_count; i++)
        if (pids[i] > 0)
            kill(pids[i], SIGSTOP);
    
    // Zatrzymanie dynamicznych stanowisk obsługi (2 i 3) uruchomionych przez menedżera
    if (global_shm != NULL)
    {
        for (int i = 1; i < NUM_SERVICE_STAFF; i++)
        {
            if (global_shm->service_pids[i] > 0)
                kill(global_shm->service_pids[i], SIGSTOP);
        }
    }
    
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
}

// Handler SIGCONT - wznowienie symulacji
// Wywoływany automatycznie gdy proces jest wznawiany (przez fg lub kill -CONT)
void sigcont_handler(int sig)
{
    (void)sig;
    signal(SIGTSTP, sigtstp_handler);
    printf(COLOR_GREEN ">>>" COLOR_RESET " [MAIN] WZNOWIENIE " COLOR_GREEN "<<<" COLOR_RESET "\n\n");
    
    // Wznowienie procesów uruchomionych przez main
    for (int i = 0; i < pid_count; i++)
        if (pids[i] > 0)
            kill(pids[i], SIGCONT);
    
    // Wznowienie dynamicznych stanowisk obsługi (2 i 3) uruchomionych przez menedżera
    if (global_shm != NULL)
    {
        for (int i = 1; i < NUM_SERVICE_STAFF; i++)
        {
            if (global_shm->service_pids[i] > 0)
                kill(global_shm->service_pids[i], SIGCONT);
        }
    }
}

int main()
{
    srand(time(NULL));

    // Ustawienie handlerów sygnałów
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCONT, sigcont_handler);

    // Inicjalizacja IPC
    key_t key_msg = ftok(FTOK_PATH, PROJ_ID_MSG);
    key_t key_shm = ftok(FTOK_PATH, PROJ_ID_SHM);
    key_t key_sem = ftok(FTOK_PATH, PROJ_ID_SEM);

    // Używamy safe wrapperów
    msg_id = safe_msgget(key_msg, IPC_CREAT | 0666);
    shm_id = safe_shmget(key_shm, sizeof(shared_data), IPC_CREAT | 0666);
    sem_id = safe_semget(key_sem, SEM_NUM, IPC_CREAT | 0666);

    // Inicjalizacja pamięci dzielonej
    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);
    global_shm = shm; // Ustawienie globalnego wskaźnika dla handlerów sygnałów
    memset(shm, 0, sizeof(shared_data));

    // Ustawienia początkowe
    shm->simulation_running = 1;
    shm->start_time = time(NULL);

    // Inicjalizacja Pamięci PID-ów obsługi (0 = brak procesu)
    shm->service_pids[0] = 0; // zostanie ustawione zaraz
    shm->service_pids[1] = 0;
    shm->service_pids[2] = 0;

    // Inicjalizacja semaforów
    semctl(sem_id, SEM_LOG, SETVAL, 1);
    semctl(sem_id, SEM_PROC_LIMIT, SETVAL, 5000);
    semctl(sem_id, SEM_QUEUE, SETVAL, 100);
    semctl(sem_id, SEM_MECHANIC_ASSIGN, SETVAL, 1); // Semafor binarny dla przypisywania mechaników

    // Inicjalizacja pliku raportu
    FILE *f = fopen("raport.txt", "w");
    if (f)
    {
        fprintf(f, "START SYMULACJI (PID MAIN: %d)\n", getpid());
        fclose(f);
    }

    // Przygotowanie argumentów dla exec
    char msg_id_str[16], shm_id_str[16], sem_id_str[16];
    snprintf(msg_id_str, sizeof(msg_id_str), "%d", msg_id);
    snprintf(shm_id_str, sizeof(shm_id_str), "%d", shm_id);
    snprintf(sem_id_str, sizeof(sem_id_str), "%d", sem_id);

    // Mechanicy
    for (int i = 1; i <= NUM_MECHANICS; i++)
    {
        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%d", i);

        pid_t p = safe_fork();
        if (p == 0)
        {
            reset_child_signals();
            execl(MECHANIC_EXEC_PATH, "mechanic", id_str, msg_id_str, shm_id_str, sem_id_str, NULL);
            perror("execl mechanic failed");
            exit(EXIT_FAILURE);
        }
        pids[pid_count++] = p;
    }

    // Obsługa (stanowisko 1 - stałe)
    pid_t ps1 = safe_fork();
    if (ps1 == 0)
    {
        reset_child_signals();
        execl(SERVICE_EXEC_PATH, "service", "1", msg_id_str, shm_id_str, sem_id_str, NULL);
        perror("execl service failed");
        exit(EXIT_FAILURE);
    }
    pids[pid_count++] = ps1;
    shm->service_pids[0] = ps1; // Rejestrujemy PID stałego pracownika

    // Kasjer
    pid_t pc = safe_fork();
    if (pc == 0)
    {
        reset_child_signals();
        execl(CASHIER_EXEC_PATH, "cashier", msg_id_str, shm_id_str, sem_id_str, NULL);
        perror("execl cashier failed");
        exit(EXIT_FAILURE);
    }
    pids[pid_count++] = pc;

    // Generator klientów
    pid_t pg = safe_fork();
    if (pg == 0)
    {
        reset_child_signals();
        execl(GENERATOR_EXEC_PATH, "generator", msg_id_str, shm_id_str, sem_id_str, NULL);
        perror("execl generator failed");
        exit(EXIT_FAILURE);
    }
    pids[pid_count++] = pg;

    // Menedżer
    pid_t pm = safe_fork();
    if (pm == 0)
    {
        reset_child_signals();
        execl(MANAGER_EXEC_PATH, "manager", shm_id_str, sem_id_str, msg_id_str, NULL);
        perror("execl manager failed");
        exit(EXIT_FAILURE);
    }
    pids[pid_count++] = pm;

    printf(COLOR_GREEN "Symulacja działa." COLOR_RESET "\n");
    printf(COLOR_GRAY "[INFO]" COLOR_RESET " CTRL+C = zakończ | CTRL+Z = wstrzymaj | fg = wznów\n\n");

    // Główna pętla symulacji - wyświetlanie statusu i monitorowanie procesów
    while (shm->simulation_running)
    {
        // Opóźnienie 1 sekundy między aktualizacjami statusu
        custom_usleep(1000000);

        // Obliczenie czasu symulacji na podstawie upływu czasu rzeczywistego
        // Funkcja get_simulation_time() oblicza czas na bieżąco,
        // dzięki czemu po wznowieniu (SIGCONT) czas jest poprawny
        sim_time st = get_simulation_time(shm->start_time);
        int open = (st.hour >= OPEN_HOUR && st.hour < CLOSE_HOUR);

        printf("\r" COLOR_CYAN "[MAIN]" COLOR_RESET " Dzień %d | Czas symulacji: " COLOR_WHITE "%02d:%02d" COLOR_RESET "%s" COLOR_RESET " | Klienci w kolejce: " COLOR_YELLOW "%d" COLOR_RESET " \n",
               st.day + 1, st.hour, st.minute,
               (open ? COLOR_GREEN " (OTWARTE)" : COLOR_RED " (ZAMKNIĘTE)"),
               shm->clients_in_queue);
        fflush(stdout);

        // Sprawdzenie czy któryś proces potomny nie zakończył się nieoczekiwanie
        int status;
        pid_t dead_pid;
        while ((dead_pid = waitpid(-1, &status, WNOHANG)) > 0)
        {
            // Sprawdź czy to jest jeden z naszych kluczowych procesów
            int is_critical = 0;
            for (int i = 0; i < pid_count; i++)
            {
                if (pids[i] == dead_pid)
                {
                    is_critical = 1;
                    pids[i] = 0; // Oznacz jako nieaktywny
                    break;
                }
            }

            // Jeśli to nie jest nasz proces (np. proces klienta), pomijamy
            if (!is_critical)
                continue;

            // Jeśli proces zakończył się z błędem (kod wyjścia != 0)
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            {
                printf("\n" COLOR_RED "[MAIN]" COLOR_RESET " Wykryto awarię procesu potomnego PID %d. Kod wyjścia: %d\n", dead_pid, WEXITSTATUS(status));
                shm->simulation_running = 0; // Przerwanie symulacji
            }
            else if (WIFSIGNALED(status))
            {
                // Proces kluczowy został zabity sygnałem - nieautoryzowane zakończenie
                int sig = WTERMSIG(status);
                printf("\n" COLOR_RED "[MAIN]" COLOR_RESET " Wykryto nieautoryzowane zabicie procesu PID %d sygnałem %d (%s)\n", 
                       dead_pid, sig, strsignal(sig));
                printf(COLOR_RED "[MAIN]" COLOR_RESET " Kończenie symulacji z powodu nieautoryzowanego zakończenia procesu...\n");
                shm->simulation_running = 0; // Przerwanie symulacji
            }
            else if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            {
                // Proces kluczowy zakończył się normalnie - to też nie powinno się zdarzyć
                printf("\n" COLOR_RED "[MAIN]" COLOR_RESET " Proces kluczowy PID %d zakończył się nieoczekiwanie (kod 0)\n", dead_pid);
                shm->simulation_running = 0; // Przerwanie symulacji
            }
        }
    }

    cleanup();
    return 0;
}