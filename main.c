#include "common.h"

int msg_id, shm_id, sem_id;
pid_t pids[100];
int pid_count = 0;

// Ścieżki do programów wykonywalnych
#define MECHANIC_EXEC_PATH  "./mechanic"
#define SERVICE_EXEC_PATH   "./service"
#define CASHIER_EXEC_PATH   "./cashier"
#define MANAGER_EXEC_PATH   "./manager"
#define GENERATOR_EXEC_PATH "./generator"

void cleanup()
{
    printf("\n" COLOR_YELLOW "[MAIN]" COLOR_RESET " Zamykanie symulacji...\n");
    for (int i = 0; i < pid_count; i++)
    {
        if (pids[i] > 0)
            kill(pids[i], SIGTERM);
    }
    // Chwila dla menedżera na zabicie jego dzieci
    custom_usleep(1000000);

    msgctl(msg_id, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    printf(COLOR_GREEN "[MAIN]" COLOR_RESET " Zasoby zwolnione. Koniec.\n");
}

void reset_child_signals()
{
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
}

void sigint_handler(int sig)
{
    (void)sig;
    cleanup();
    exit(0);
}

void sigtstp_handler(int sig)
{
    (void)sig;
    printf("\n\n" COLOR_ORANGE ">>>" COLOR_RESET " [MAIN] WSTRZYMANIE (CTRL+Z) " COLOR_ORANGE "<<<" COLOR_RESET "\n");
    for (int i = 0; i < pid_count; i++)
        kill(pids[i], SIGSTOP);
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
}

void sigcont_handler(int sig)
{
    (void)sig;
    signal(SIGTSTP, sigtstp_handler);
    printf(COLOR_GREEN ">>>" COLOR_RESET " [MAIN] WZNOWIENIE " COLOR_GREEN "<<<" COLOR_RESET "\n\n");
    for (int i = 0; i < pid_count; i++)
        kill(pids[i], SIGCONT);
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
    memset(shm, 0, sizeof(shared_data));

    // Ustawienia początkowe
    shm->simulation_running = 1;
    shm->start_time = time(NULL);
    shm->current_hour = 7;

    // Inicjalizacja Pamięci PID-ów obsługi (0 = brak procesu)
    shm->service_pids[0] = 0; // zostanie ustawione zaraz
    shm->service_pids[1] = 0;
    shm->service_pids[2] = 0;

    // Inicjalizacja semaforów
    semctl(sem_id, SEM_LOG, SETVAL, 1);
    semctl(sem_id, SEM_PROC_LIMIT, SETVAL, 100);
    semctl(sem_id, SEM_QUEUE, SETVAL, 20);

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

    while (shm->simulation_running)
    {
        custom_usleep(1000000);

        // Aktualizacja czasu symulacji,
        // oparta na czasie rzeczywistym od startu
        // jest to rozwiązanie optymalne.
        time_t now = time(NULL);
        double seconds_elapsed = difftime(now, shm->start_time);
        long total_sim_minutes = (long)(seconds_elapsed * SIM_MINUTES_PER_REAL_SEC);
        int total_hours = 7 + (total_sim_minutes / 60);
        shm->current_hour = total_hours % 24;
        shm->current_minute = total_sim_minutes % 60;
        int current_day = total_hours / 24;

        printf("\r" COLOR_CYAN "[MAIN]" COLOR_RESET " Dzień %d | Czas symulacji: " COLOR_WHITE "%02d:%02d" COLOR_RESET " | Klienci w kolejce: " COLOR_YELLOW "%d" COLOR_RESET " ",
               current_day + 1, shm->current_hour, shm->current_minute, shm->clients_in_queue);
        fflush(stdout);

        // Sprawdzamy czy któryś proces nie umarł (np. przez panic)
        int status;
        pid_t dead_pid = waitpid(-1, &status, WNOHANG);
        if (dead_pid > 0)
        {
            // Jeśli proces umarł z błędem (nie 0), kończymy wszystko
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            {
                printf("\n" COLOR_RED "[MAIN]" COLOR_RESET " Wykryto awarię procesu potomnego PID %d. Kod wyjścia: %d\n", dead_pid, WEXITSTATUS(status));
                shm->simulation_running = 0; // Przerwij pętlę
            }
        }
    }

    cleanup();
    return 0;
}