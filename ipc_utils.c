#include "common.h"
#include "ipc_utils.h"

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

void init_queue()
{
    msg_queue_id = msgget(QUEUE_KEY, IPC_CREAT | 0666);
    if (msg_queue_id == -1)
    {
        perror("Błąd: nie powiodło się tworzenie kolejki.");
        exit(1);
    }
    printf("[System] Utworzono kolejkę komunikatów o id: %d\n", msg_queue_id);
}

void clean_queue()
{
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
    {
        perror("Błąd: nie powiodło się usuwanie kolejki.");
    }
    else
    {
        printf("[System] Usunięto kolejkę komunikatów.\n");
    }
}

void init_semaphores()
{
    sem_id = semget(SEM_KEY, NUM_MECHANICS, IPC_CREAT | 0666);
    if (sem_id == -1)
    {
        perror("Błąd tworzenia semaforów");
        exit(1);
    }

    union semun arg;
    unsigned short values[NUM_MECHANICS];
    for (int i = 0; i < NUM_MECHANICS; i++)
        values[i] = 1; // 1 = wolne
    arg.array = values;

    if (semctl(sem_id, 0, SETALL, arg) == -1)
    {
        perror("Błąd inicjalizacji semaforów");
        exit(1);
    }
    printf("[System] Utworzono i zainicjalizowano %d semaforów.\n", NUM_MECHANICS);
}

void clean_semaphores()
{
    if (semctl(sem_id, 0, IPC_RMID) == -1)
    {
        perror("Błąd: nie powiodło się usuwanie semaforów");
    }
    else
    {
        printf("[System] Usunięto zbiór semaforów.\n");
    }
}

int reserve_station(char brand)
{
    struct sembuf sb;
    sb.sem_flg = IPC_NOWAIT;
    sb.sem_op = -1; // Próba zajęcia

    // Jeżeli marka to U lub Y, sprawdzamy najpierw stanowisko 8 (indeks 7).
    if (brand == 'U' || brand == 'Y')
    {
        sb.sem_num = 7;
        if (semop(sem_id, &sb, 1) != -1)
            return 7;
    }

    // Sprawdzamy wszystkie stanowiska po kolei w poszukiwaniu wolnego.
    for (int i = 0; i < 7; i++)
    {
        sb.sem_num = i;
        if (semop(sem_id, &sb, 1) != -1)
            return i;
    }

    // Żadne stanowisko nie jest wolne
    return -1;
}

void release_station(int mechanic_id)
{
    struct sembuf sb;
    sb.sem_num = mechanic_id;
    sb.sem_op = 1; // Zwolnienie
    sb.sem_flg = 0;

    if (semop(sem_id, &sb, 1) == -1)
    {
        perror("[Mechanik] Błąd zwalniania semafora.");
    }
}

int shm_time_id = -1;

void init_shared_time()
{
    shm_time_id = shmget(SHM_TIME_KEY, sizeof(SharedTime), IPC_CREAT | 0666);
    if (shm_time_id == -1)
    {
        perror("Błąd tworzenia pamięci dzielonej czasu.");
        exit(1);
    }

    SharedTime *time_ptr = (SharedTime *)shmat(shm_time_id, NULL, 0);
    if (time_ptr == (void *)-1)
    {
        perror("Błąd przyłączania czasu.");
        exit(1);
    }
    time_ptr->current_hour = 6;
    shmdt(time_ptr);
    printf("[System] Zegar zainicjalizowany.\n");
}

SharedTime *attach_shared_time()
{
    int id = shmget(SHM_TIME_KEY, sizeof(SharedTime), 0666);
    if (id == -1)
        return NULL;
    SharedTime *ptr = (SharedTime *)shmat(id, NULL, 0);
    return (ptr == (void *)-1 ? NULL : ptr);
}

void remove_shared_time()
{
    int id = shmget(SHM_TIME_KEY, sizeof(SharedTime), 0666);
    if (id != -1)
        shmctl(id, IPC_RMID, NULL);
}

int shm_pids_id = -1;

SharedData *attach_shared_pids()
{
    int id = shmget(SHM_PIDS_KEY, sizeof(SharedData), 0666);
    if (id == -1)
        return NULL;
    return (SharedData *)shmat(id, NULL, 0);
}

void init_shared_pids()
{
    shm_pids_id = shmget(SHM_PIDS_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_pids_id == -1)
    {
        perror("Błąd tworzenia pamięci dzielonej dla pid");
        exit(1);
    }
    SharedData *data = (SharedData *)shmat(shm_pids_id, NULL, 0);
    memset(data, 0, sizeof(SharedData));
    shmdt(data);
    printf("[System] Pamięć dzielona dla pid zainicjalizowana.\n");
}

void remove_shared_pids()
{
    int id = shmget(SHM_PIDS_KEY, sizeof(SharedData), 0666);
    if (id != -1)
        shmctl(id, IPC_RMID, NULL);
}