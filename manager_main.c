#include "common.h"

void run_manager(int shm_id, int sem_id, int msg_id);

// Punkt wejścia programu menedżera
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <shm_id> <sem_id> <msg_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int shm_id = atoi(argv[1]);
    int sem_id = atoi(argv[2]);
    int msg_id = atoi(argv[3]);

    srand(time(NULL) ^ getpid());
    run_manager(shm_id, sem_id, msg_id);

    return 0;
}
