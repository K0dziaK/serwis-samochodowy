#include "common.h"

// Parametry:
//   sem_id - id semafora
//   shm - wskaźnik do pamięci dzielonej (może być NULL jeśli już odłączona)
//   in_queue - czy klient jest w kolejce (wymaga zwolnienia SEM_QUEUE i dekrementacji licznika)
//   shm_attached - czy pamięć dzielona jest nadal przyłączona (wymaga shmdt)
void client_exit(int sem_id, shared_data *shm, int in_queue, int shm_attached)
{
    if (in_queue && shm_attached && shm != NULL)
    {
        sem_lock(sem_id, SEM_LOG);
        shm->clients_in_queue--;
        sem_unlock(sem_id, SEM_LOG);
        sem_unlock(sem_id, SEM_QUEUE);
    }
    if (shm_attached && shm != NULL)
    {
        shmdt(shm);
    }
    // ZAWSZE zwalniamy slot w limicie procesów
    sem_unlock(sem_id, SEM_PROC_LIMIT);
    exit(0);
}

void run_client(int msg_id, int shm_id, int sem_id)
{
    // Ignoruj sygnały - klient musi dokończyć flow (w tym zapłatę) przed zakończeniem
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    
    srand(getpid());
    global_sem_id = sem_id;
    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);
    
    // Zapamiętanie start_time
    time_t start_time = shm->start_time;

    // Losowanie marki
    char brands[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char brand = brands[rand() % 26];

    // Losowanie usterki z cennika
    int s_id = rand() % NUM_SERVICES;

    // Sprawdzenie krytyczności na podstawie cennika
    int is_critical = PRICE_LIST[s_id].is_critical;

    // Obliczenie aktualnego czasu symulacji
    sim_time st = get_simulation_time(start_time);
    int open = (st.hour >= OPEN_HOUR && st.hour < CLOSE_HOUR);

    // Decyzja o wejściu do kolejki
    if (!open)
    {
        // Jeśli naprawa nie jest krytyczna i do otwarcia zostało dużo czasu (> 1h) -> odjazd
        int hours_to_open = (OPEN_HOUR - st.hour);
        if (hours_to_open < 0)
            hours_to_open += 24; // następny dzień

        if (!is_critical && hours_to_open > T1)
        {
            // Odjazd - klient nie jest w kolejce, pamięć nadal przyłączona
            client_exit(sem_id, shm, 0, 1);
        }
    }

    // Próba wejścia do kolejki
    struct sembuf sb = {SEM_QUEUE, -1, 0};
    if (semop(sem_id, &sb, 1) == -1)
    {
        // Błąd semop - nie udało się wejść do kolejki
        client_exit(sem_id, shm, 0, 1);
    }

    // Jesteśmy w kolejce
    sem_lock(sem_id, SEM_LOG);
    shm->clients_in_queue++;
    sem_unlock(sem_id, SEM_LOG);

    // Przygotowanie komunikatu zgłoszeniowego
    msg_buf msg;
    msg.mtype = MSG_CLIENT_TO_SERVICE;
    msg.client_pid = getpid();
    msg.brand = brand;
    msg.service_id = s_id;
    msg.sender_pid = getpid();

    // Wysłanie zgłoszenia
    safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

    // Po wysłaniu zgłoszenia - klient jest "przekazany" do obsługi
    // Od tego momentu to obsługa jest odpowiedzialna za zwolnienie SEM_QUEUE i dekrementację licznika
    // Klient tylko czeka na odpowiedź

    // Oczekiwanie na ofertę od obsługi
    if (safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_TO_CLIENT + getpid()) == -1)
    {
        // Błąd odbioru
        client_exit(sem_id, shm, 0, 1);
    }

    if (msg.decision == 0)
    {
        // Marka nieobsługiwana lub odmowa przez obsługę
        client_exit(sem_id, shm, 0, 1);
    }

    // Decyzja klienta (2% odrzuca warunki: cenę/czas)
    if ((rand() % 100) < CHANCE_RESIGNATION)
    {
        // Odrzucenie oferty
        msg.decision = 0;
        msg.mtype = MSG_BASE_CLIENT_DECISION + getpid();
        safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
        client_exit(sem_id, shm, 0, 1);
    }

    // Akceptacja
    msg.decision = 1;
    msg.mtype = MSG_BASE_CLIENT_DECISION + getpid();
    safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

    // Pętla oczekiwania na rozwój zdarzeń
    // Możliwa dodatkowa usterka lub zakończenie naprawy
    while (1)
    {
        // Oczekiwanie wiadomości
        if (safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_TO_CLIENT + getpid()) == -1)
            break;

        if (msg.is_extra_repair)
        {
            // Decyzja o extra naprawie (20% odmawia)
            if ((rand() % 100) < CHANCE_EXTRA_REPAIR_REFUSAL)
                msg.decision = 0;
            else
                msg.decision = 1;

            msg.mtype = MSG_BASE_CLIENT_DECISION + getpid();
            safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
        }
        else
        {
            // To rachunek od kasjera
            break;
        }
    }

    // Płatność
    msg.mtype = MSG_BASE_CLIENT_PAYMENT + getpid();
    safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

    // Zakończenie - klient odchodzi
    client_exit(sem_id, shm, 0, 1);
}