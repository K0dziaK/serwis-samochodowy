#include "common.h"

void run_client(int msg_id, int shm_id, int sem_id);

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
    run_client(msg_id, shm_id, sem_id);

    return 0;
}
