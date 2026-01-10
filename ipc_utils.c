#include "common.h"
#include "ipc_utils.h"

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

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