#include "common.h"

// Ścieżka do programu obsługi klienta
#define SERVICE_EXEC_PATH "./service"

// Przygotowanie procesu potomnego (stanowiska dynamicznego)
void prepare_dynamic_child()
{
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
}

// Flaga kontynuacji pracy menedżera
volatile int keep_running = 1;

// Handler sygnału SIGTERM
void mgr_sigterm(int sig)
{
    (void)sig;
    keep_running = 0;
}

// Główna funkcja menedżera - zarządza stanowiskami obsługi i mechaników
void run_manager(int shm_id, int sem_id, int msg_id)
{
    global_sem_id = sem_id;
    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);
    
    // Kopiowanie czasu startu symulacji
    time_t start_time = shm->start_time;

    // Przygotowanie argumentów dla exec (konwersja int -> string)
    char msg_id_str[16], shm_id_str[16], sem_id_str[16];
    snprintf(msg_id_str, sizeof(msg_id_str), "%d", msg_id);
    snprintf(shm_id_str, sizeof(shm_id_str), "%d", shm_id);
    snprintf(sem_id_str, sizeof(sem_id_str), "%d", sem_id);

    // Rejestracja handlera SIGTERM
    signal(SIGTERM, mgr_sigterm);
    log_color(ROLE_MANAGER, "MANAGER: Start.");

    // Bufor na komendy sterujące z klawiatury
    char buffer[100];
    fd_set readfds;
    struct timeval tv;

    // Główna pętla zarządzania
    while (keep_running && shm->simulation_running)
    {
        // Sprawdzanie wejścia z klawiatury (sterowanie ręczne)
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // Nieblokujące czytanie z klawiatury
        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds))
        {
            int bytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            if (bytes > 0)
            {
                buffer[bytes] = 0;
                int sig_num, mech_id;
                // Parsowanie komendy: numer_sygnału numer_mechanika
                if (sscanf(buffer, "%d %d", &sig_num, &mech_id) >= 1)
                {
                    // Komenda 4 - pożar (zamknięcie całego serwisu)
                    if (sig_num == 4)
                    {
                        log_color(ROLE_MANAGER, "MANAGER: POŻAR! Zamykam serwis.");
                        for (int i = 0; i < NUM_MECHANICS; i++)
                            if (shm->mechanics_pids[i] > 0)
                                kill(shm->mechanics_pids[i], SIG_FIRE);
                        shm->simulation_running = 0;
                    }
                    // Komendy 1-3 dla konkretnego mechanika
                    else if (mech_id >= 1 && mech_id <= NUM_MECHANICS)
                    {
                        // Ignorowanie zamknięcia już zamkniętego stanowiska
                        if (sig_num == 1 && shm->mechanic_status[mech_id - 1] == 2)
                            continue;
                        pid_t target = shm->mechanics_pids[mech_id - 1];
                        if (target > 0)
                        {
                            int s = 0;
                            if (sig_num == 1) s = SIG_CLOSE_STATION;  // Zamknięcie stanowiska
                            if (sig_num == 2) s = SIG_SPEED_UP;       // Przyspieszenie
                            if (sig_num == 3) s = SIG_RESTORE_SPEED;  // Normalna prędkość
                            if (s) kill(target, s);
                        }
                    }
                }
            }
        }

        // Obliczanie aktualnego czasu symulacji
        sim_time st = get_simulation_time(start_time);
        
        // Zarządzanie stanowiskami obsługi na podstawie długości kolejki
        int q = shm->clients_in_queue;
        int is_open_time = (st.hour >= OPEN_HOUR && st.hour < CLOSE_HOUR);

        // Stanowisko 2 - uruchamiane gdy kolejka > K1
        if (shm->service_pids[1] == 0 && q > K1 && is_open_time)
        {
            log_color(ROLE_MANAGER, "MANAGER: Kolejka %d > %d. Uruchamiam Obsługę 2.", q, K1);
            pid_t p = safe_fork();
            if (p == 0)
            {
                prepare_dynamic_child();
                execl(SERVICE_EXEC_PATH, "service", "2", msg_id_str, shm_id_str, sem_id_str, NULL);
                perror("execl service 2 failed");
                exit(EXIT_FAILURE);
            }
            shm->service_pids[1] = p;
        }
        else if (shm->service_pids[1] > 0)
        {
            // Sprawdzanie czy stanowisko 2 zakończyło pracę
            int status;
            if (waitpid(shm->service_pids[1], &status, WNOHANG) > 0)
            {
                shm->service_pids[1] = 0;
            }
            // Zamykanie stanowiska 2 gdy kolejka <= 2 lub poza godzinami
            else if (q <= 2 || !is_open_time)
            {
                kill(shm->service_pids[1], SIGTERM);
                waitpid(shm->service_pids[1], NULL, 0);
                shm->service_pids[1] = 0;
            }
        }

        // Stanowisko 3 - uruchamiane gdy kolejka > K2
        if (shm->service_pids[2] == 0 && q > K2 && is_open_time)
        {
            log_color(ROLE_MANAGER, "MANAGER: Kolejka %d > %d. Uruchamiam Obsługę 3.", q, K2);
            pid_t p = safe_fork();
            if (p == 0)
            {
                prepare_dynamic_child();
                execl(SERVICE_EXEC_PATH, "service", "3", msg_id_str, shm_id_str, sem_id_str, NULL);
                perror("execl service 3 failed");
                exit(EXIT_FAILURE);
            }
            shm->service_pids[2] = p;
        }
        else if (shm->service_pids[2] > 0)
        {
            // Sprawdzanie czy stanowisko 3 zakończyło pracę
            int status;
            if (waitpid(shm->service_pids[2], &status, WNOHANG) > 0)
            {
                shm->service_pids[2] = 0;
            }
            // Zamykanie stanowiska 3 gdy kolejka <= 3 lub poza godzinami
            else if (q <= 3 || !is_open_time)
            {
                kill(shm->service_pids[2], SIGTERM);
                waitpid(shm->service_pids[2], NULL, 0);
                shm->service_pids[2] = 0;
            }
        }

        // Sprawdzanie czy wszystkie stanowiska mechaników zamknięte
        int all_mech_closed = 1;
        for (int i = 0; i < NUM_MECHANICS; i++)
        {
            if (shm->mechanic_status[i] != 2)
            {
                all_mech_closed = 0;
                break;
            }
        }

        // Zamknięcie symulacji gdy wszyscy mechanicy zamknięci
        if (all_mech_closed)
        {
            log_color(ROLE_MANAGER, "MANAGER: Wszystkie stanowiska mechaników zamknięte. Zamykam system.");
            shm->simulation_running = 0;
        }
    }

    // Zamykanie dynamicznych stanowisk obsługi
    for (int i = 1; i < NUM_SERVICE_STAFF; i++)
    {
        if (shm->service_pids[i] > 0)
        {
            kill(shm->service_pids[i], SIGTERM);
            waitpid(shm->service_pids[i], NULL, 0);
        }
    }

    // Opróżnianie kolejki klientów przy zamknięciu
    msg_buf msg;
    while (msgrcv(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_CLIENT_TO_SERVICE, IPC_NOWAIT) != -1)
    {
        // Zwalnianie slotu w kolejce
        sem_lock(sem_id, SEM_LOG);
        if (shm->clients_in_queue > 0)
            shm->clients_in_queue--;
        sem_unlock(sem_id, SEM_LOG);
        sem_unlock(sem_id, SEM_QUEUE);
        // Wysyłanie odmowy do klienta
        msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
        msg.decision = 0;
        msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
    }

    shmdt(shm);
    exit(0);
}