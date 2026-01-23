#include "common.h"

#define CLIENT_EXEC_PATH "./client"

void run_generator(int msg_id, int shm_id, int sem_id)
{
    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);

    // Przygotowanie argumentów dla exec
    char msg_id_str[16], shm_id_str[16], sem_id_str[16];
    snprintf(msg_id_str, sizeof(msg_id_str), "%d", msg_id);
    snprintf(shm_id_str, sizeof(shm_id_str), "%d", shm_id);
    snprintf(sem_id_str, sizeof(sem_id_str), "%d", sem_id);

    while (shm->simulation_running)
    {
        sem_lock(sem_id, SEM_PROC_LIMIT);

        pid_t pid = safe_fork();
        if (pid == 0)
        {
            signal(SIGTSTP, SIG_IGN);

            // Użycie exec() zamiast bezpośredniego wywołania funkcji
            execl(CLIENT_EXEC_PATH, "client", msg_id_str, shm_id_str, sem_id_str, NULL);
            
            // Jeśli exec się nie powiedzie
            perror("execl client failed");
            exit(EXIT_FAILURE);
        }

        custom_usleep((rand() % 2 + 1) * 100000); // Losowo co 0.1-0.3 sekundy
    }
    shmdt(shm);
    exit(0);
}

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
