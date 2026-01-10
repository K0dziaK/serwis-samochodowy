#include "common.h"
#include "ipc_utils.h"
#include "roles.h"

void run_mechanic(int id)
{
    CarMessage msg;
    long my_msg_type = MSG_TYPE_MECHANIC_OFFSET + id;
    srand(time(NULL) ^ getpid());

    printf("[Mechanik %d] Obsługuje stanowisko %d.\n", getpid(), id);

    while (1)
    {
        // Oczekiwanie na samochód przypisany do tego stanowiska
        if (msgrcv(msg_queue_id, &msg, sizeof(CarData), my_msg_type, 0) == -1)
        {
            if (errno == EIDRM || errno == EINTR)
                break;
            printf("[Mechanik %d] Błąd msgrcv", id + 1);
            exit(1);
        }

        printf("    -> [Mechanik %d] Rozpoczynam naprawę auta ID: %d (%s). Czas: %ds. Koszt: %d PLN\n",
               id + 1,
               msg.data.id,
               msg.data.service_name,
               msg.data.time_est,
               msg.data.cost);

        // Symulacja pierwszej fazy naprawy
        int first_phase = msg.data.time_est / 2;
        if (first_phase < 1) first_phase = 1;
        sleep(first_phase);
        
        int remaining_time = msg.data.time_est - first_phase;

        // Losowanie dodatkowej usterki
        if (rand() % 100 < CHANCE_EXTRA_FAULT)
        {
            printf("    -> [Mechanik %d] Znaleziono dodatkową usterkę (ID: %d)! Konsultacja z obsługą.\n", id + 1, msg.data.id);

            int extra_cost = 200 + (rand() % 300);
            int extra_time = 2 + (rand() % 3);

            CarMessage consult_msg = msg;
            consult_msg.mtype = MSG_TYPE_CONSULTATION;
            consult_msg.data.mechanic_pid = getpid();
            consult_msg.data.mechanic_id = id;
            consult_msg.data.cost = extra_cost;
            consult_msg.data.time_est = extra_time;

            // Wysłanie zapytania do obsługi
            if (msgsnd(msg_queue_id, &consult_msg, sizeof(CarData), 0) != -1)
            {
                // Oczekiwanie na odpowiedź
                if (msgrcv(msg_queue_id, &consult_msg, sizeof(CarData), getpid(), 0) != -1)
                {
                    if (consult_msg.data.extra_accepted)
                    {
                        printf("    -> [Mechanik %d] Klient zaakceptował dopłatę (+%d PLN). Kontynuuję naprawę.\n", id + 1, extra_cost);
                        msg.data.cost += extra_cost;
                        remaining_time += extra_time;
                    }
                    else
                    {
                        printf("    -> [Mechanik %d] Klient odrzucił dopłatę. Naprawiam tylko usterkę podstawową.\n", id + 1);
                    }
                }
            }
        }

        // Dokończenie naprawy
        if (remaining_time > 0)
            sleep(remaining_time);

        printf("    -> [Mechanik %d] Koniec naprawy auta ID: %d.\n", id + 1, msg.data.id);

        release_station(id);

        // Wysłanie informacji do kasy
        msg.mtype = MSG_TYPE_BILLING;
        msgsnd(msg_queue_id, &msg, sizeof(CarData), 0);
    }
    exit(0);
}