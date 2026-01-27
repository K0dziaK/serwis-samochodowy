#include "common.h"

// Kasjer - obsługuje płatności klientów po naprawie
void run_cashier(int msg_id, int shm_id, int sem_id)
{
    global_sem_id = sem_id;
    shared_data *shm = (shared_data *)shmat(shm_id, NULL, 0);
    msg_buf msg;

    log_color(ROLE_CASHIER, "Kasjer %d: Otwieram kasę.", getpid());

    // Główna pętla pracy kasjera
    while (shm->simulation_running)
    {
        // Czekanie na komunikat od obsługi (gotowe auto lub klient odprawiony)
        if (safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_SERVICE_TO_CASHIER) == -1)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        // Sprawdzenie czy klient został odprawiony bez naprawy (brak wolnych mechaników)
        if (msg.cost == 0)
        {
            log_color(ROLE_CASHIER, "Kasjer: Klient %d odprawiony bez naprawy. Zwracam kluczyki.", msg.client_pid);
            msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
            safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

            // Czekanie na potwierdzenie odbioru od klienta
            msg_buf payment;
            safe_msgrcv_wait(msg_id, &payment, sizeof(msg_buf) - sizeof(long), MSG_BASE_CLIENT_PAYMENT + msg.client_pid);
            continue;
        }

        log_color(ROLE_CASHIER, "Kasjer: Przygotowuję rachunek dla klienta %d. Kwota: %d", msg.client_pid, msg.cost);

        // Wysłanie rachunku do klienta
        msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
        safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

        // Czekanie na płatność od klienta
        msg_buf payment;
        safe_msgrcv_wait(msg_id, &payment, sizeof(msg_buf) - sizeof(long), MSG_BASE_CLIENT_PAYMENT + msg.client_pid);

        log_color(ROLE_CASHIER, "Kasjer: Otrzymano wpłatę od klienta %d. Wydaję kluczyki.", msg.client_pid);
    }
    shmdt(shm);
    exit(0);
}