#include "common.h"

void run_service(int staff_id, int msg_id, int shm_id, int sem_id);

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <staff_id> <msg_id> <shm_id> <sem_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int staff_id = atoi(argv[1]);
    int msg_id = atoi(argv[2]);
    int shm_id = atoi(argv[3]);
    int sem_id = atoi(argv[4]);

    srand(time(NULL) ^ getpid());
    run_service(staff_id, msg_id, shm_id, sem_id);

    return 0;
}
