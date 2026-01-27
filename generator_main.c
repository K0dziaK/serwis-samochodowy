#include "common.h"

#define CLIENT_EXEC_PATH "./client"

// Handler SIGCHLD - zbieranie procesów klientów
void sigchld_handler(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Generator klientów - tworzy nowe procesy klientów w losowych odstępach
void run_generator(int msg_id, int shm_id, int sem_id)
{
    // Rejestracja handlera SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);

    // Konwersja ID-ów IPC na stringi dla exec
    char msg_id_str[16], shm_id_str[16], sem_id_str[16];
    snprintf(msg_id_str, sizeof(msg_id_str), "%d", msg_id);
    snprintf(shm_id_str, sizeof(shm_id_str), "%d", shm_id);
    snprintf(sem_id_str, sizeof(sem_id_str), "%d", sem_id);

    // Główna pętla generowania klientów
    while (shm->simulation_running)
    {
        // Czekanie na wolny slot w limicie procesów
        sem_lock(sem_id, SEM_PROC_LIMIT);

        pid_t pid = safe_fork();
        if (pid == 0)
        {
            // Proces potomny (klient) - ignoruje SIGTSTP
            signal(SIGTSTP, SIG_IGN);

            // Uruchomienie programu klienta
            execl(CLIENT_EXEC_PATH, "client", msg_id_str, shm_id_str, sem_id_str, NULL);
            
            perror("execl client failed");
            exit(EXIT_FAILURE);
        }

        // Losowe opóźnienie między kolejnymi klientami (0.1-0.3 sekundy)
        custom_usleep((rand() % 2 + 1) * 100000);
    }
    shmdt(shm);
    exit(0);
}

// Punkt wejścia programu generatora
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <msg_id> <shm_id> <sem_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int msg_id = atoi(argv[1]);
    int shm_id = atoi(argv[2]);
    int sem_id = atoi(argv[3]);

    srand(time(NULL) ^ getpid());
    run_generator(msg_id, shm_id, sem_id);

    return 0;
}
