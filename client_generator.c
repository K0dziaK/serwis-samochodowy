#include "common.h"

void run_client(int msg_id, int shm_id, int sem_id);

void run_generator(int msg_id, int shm_id, int sem_id)
{
    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);

    while (shm->simulation_running)
    {

        sem_lock(sem_id, SEM_PROC_LIMIT);

        pid_t pid = safe_fork();
        if (pid == 0)
        {
            signal(SIGTSTP, SIG_IGN);

            run_client(msg_id, shm_id, sem_id);
        }

        custom_usleep((rand() % 2 + 1) * 100000); // Losowo co 0.1-0.3 sekundy
    }
    shmdt(shm);
    exit(0);
}