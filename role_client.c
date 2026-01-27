#include "common.h"

// Funkcja pomocnicza do bezpiecznego zakończenia klienta
// Zwalnia zasoby i slot w limicie procesów
void client_exit(int sem_id, shared_data *shm, int in_queue, int shm_attached)
{
    // Zwalnianie miejsca w kolejce jeśli klient był w kolejce
    if (in_queue && shm_attached && shm != NULL)
    {
        sem_lock(sem_id, SEM_LOG);
        if (shm->clients_in_queue > 0)
            shm->clients_in_queue--;
        sem_unlock(sem_id, SEM_LOG);
        sem_unlock(sem_id, SEM_QUEUE);
    }
    // Odłączenie pamięci dzielonej
    if (shm_attached && shm != NULL)
    {
        shmdt(shm);
    }
    // Zwolnienie slotu w limicie procesów
    sem_unlock(sem_id, SEM_PROC_LIMIT);
    exit(0);
}

// Główna funkcja klienta - obsługuje cały cykl życia klienta w serwisie
void run_client(int msg_id, int shm_id, int sem_id)
{
    // Ignorowanie sygnałów - klient musi dokończyć płatność przed zakończeniem
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    
    srand(getpid());
    global_sem_id = sem_id;
    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);
    
    // Kopiowanie czasu startu symulacji
    time_t start_time = shm->start_time;

    // Losowanie marki samochodu (A-Z)
    char brands[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char brand = brands[rand() % 26];

    // Losowanie usterki z cennika
    int s_id = rand() % NUM_SERVICES;

    // Sprawdzanie krytyczności naprawy
    int is_critical = PRICE_LIST[s_id].is_critical;

    // Obliczanie aktualnego czasu symulacji
    sim_time st = get_simulation_time(start_time);
    int open = (st.hour >= OPEN_HOUR && st.hour < CLOSE_HOUR);

    // Decyzja o wejściu do kolejki
    if (!open)
    {
        // Obliczanie czasu do otwarcia
        int hours_to_open = (OPEN_HOUR - st.hour);
        if (hours_to_open < 0)
            hours_to_open += 24;

        // Klient z niekrytyczną usterką odjeżdża jeśli do otwarcia > T1 godzin
        if (!is_critical && hours_to_open > T1)
        {
            client_exit(sem_id, shm, 0, 1);
        }
    }

    // Próba wejścia do kolejki (blokująca operacja na semaforze)
    struct sembuf sb = {SEM_QUEUE, -1, 0};
    if (semop(sem_id, &sb, 1) == -1)
    {
        client_exit(sem_id, shm, 0, 1);
    }

    // Zwiększanie licznika klientów w kolejce
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

    // Wysyłanie zgłoszenia do obsługi
    safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

    // Czekanie na ofertę od obsługi
    if (safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_TO_CLIENT + getpid()) == -1)
    {
        client_exit(sem_id, shm, 0, 1);
    }

    // Sprawdzanie czy marka obsługiwana / czy nie odmówiono
    if (msg.decision == 0)
    {
        client_exit(sem_id, shm, 0, 1);
    }

    // Losowa decyzja o odrzuceniu oferty (szansa określona w common.h)
    if ((rand() % 100) < CHANCE_RESIGNATION)
    {
        msg.decision = 0;
        msg.mtype = MSG_BASE_CLIENT_DECISION + getpid();
        safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
        client_exit(sem_id, shm, 0, 1);
    }

    // Akceptacja oferty
    msg.decision = 1;
    msg.mtype = MSG_BASE_CLIENT_DECISION + getpid();
    safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

    // Pętla oczekiwania na komunikaty (dodatkowe usterki lub zakończenie naprawy)
    while (1)
    {
        // Czekanie na wiadomość od obsługi lub kasjera
        if (safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_TO_CLIENT + getpid()) == -1)
            break;

        // Sprawdzenie czy klient został odprawiony bez naprawy (brak mechaników)
        if (msg.decision == -1)
        {
            client_exit(sem_id, shm, 0, 1);
        }
        else if (msg.is_extra_repair)
        {
            // Decyzja o dodatkowej naprawie (szansa odmowy określona w common.h)
            if ((rand() % 100) < CHANCE_EXTRA_REPAIR_REFUSAL)
                msg.decision = 0;
            else
                msg.decision = 1;

            msg.mtype = MSG_BASE_CLIENT_DECISION + getpid();
            safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
        }
        else
        {
            // Rachunek od kasjera - koniec pętli
            break;
        }
    }

    // Wysyłanie potwierdzenia płatności
    msg.mtype = MSG_BASE_CLIENT_PAYMENT + getpid();
    safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

    // Zakończenie - klient opuszcza serwis
    client_exit(sem_id, shm, 0, 1);
}