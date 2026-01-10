#include "common.h"
#include "ipc_utils.h"
#include "roles.h"

int is_supported_brand(char brand)
{
    for (int i = 0; i < NUM_ALLOWED_BRANDS; i++)
    {
        if (ALLOWED_BRANDS[i] == brand)
            return 1;
    }
    return 0;
}

void run_service_worker(int id)
{
    CarMessage msg;
    srand(time(NULL) ^ getpid());
    printf("[Obsługa %d] Otworzyłem stanowisko obsługi.\n", id + 1);

    while (1)
    {
        // Priorytety obsługi: 1. Płatności, 2. Konsultacje, 3. Nowi klienci

        // Płatności
        if (msgrcv(msg_queue_id, &msg, sizeof(CarData), MSG_TYPE_BILLING, IPC_NOWAIT) != -1)
        {
            printf("[Obsługa %d] Pobieram opłatę od ID: %d. Razem %d PLN.\n", id + 1, msg.data.id, msg.data.cost);
            continue;
        }

        // Konsultacje
        if (msgrcv(msg_queue_id, &msg, sizeof(CarData), MSG_TYPE_CONSULTATION, IPC_NOWAIT) != -1)
        {
            printf("[Obsługa %d] Telefon do klienta ID: %d w sprawie dodatkowej usterki. Koszt %d PLN.\n", id + 1, msg.data.id, msg.data.cost);

            sleep(1); // Symulacja rozmowy

            if (rand() % 100 < CHANCE_REJECT_EXTRA)
            {
                printf("    -> [Obsługa %d] Klient ID: %d nie zgodził się na dodatkowe koszty.\n", id + 1, msg.data.id);
                msg.data.extra_accepted = 0;
            }
            else
            {
                printf("    -> [Obsługa %d] Klient ID: %d zgodził się na dodatkowe koszty.\n", id + 1, msg.data.id);
                msg.data.extra_accepted = 1;
            }

            msg.mtype = msg.data.mechanic_pid;
            msgsnd(msg_queue_id, &msg, sizeof(CarData), 0);
            continue;
        }

        // Nowi klienci
        if (msgrcv(msg_queue_id, &msg, sizeof(CarData), MSG_TYPE_NEW_CLIENT, IPC_NOWAIT) != -1)
        {
            printf("[Obsługa %d] Podszedł klient ID: %d, Auto: %c.\n", id + 1, msg.data.id, msg.data.brand);

            if (!is_supported_brand(msg.data.brand))
            {
                printf("    -> [Obsługa %d] Marka %c nieobsługiwana. Klient odchodzi.\n", id + 1, msg.data.brand);
                continue;
            }

            int service_id = rand() % NUM_SERVICES;
            strcpy(msg.data.service_name, SERVICE_LIST[service_id].name);
            msg.data.cost = SERVICE_LIST[service_id].base_cost;
            msg.data.time_est = SERVICE_LIST[service_id].base_time;

            printf("    -> [Obsługa %d] Propozycja: %s, Koszt: %d, Czas: %ds.\n", id + 1, msg.data.service_name, msg.data.cost, msg.data.time_est);

            if (rand() % 100 < CHANCE_REJECT_INITIAL)
            {
                printf("    -> [Obsługa %d] Klient ID: %d: Odmawia z powodu ceny.\n", id + 1, msg.data.id);
                continue;
            }

            // Próba znalezienia mechanika
            int mechanic_id = -1;
            int attempts = 0;
            while (mechanic_id == -1 && attempts < 3)
            {
                mechanic_id = reserve_station(msg.data.brand);

                if (mechanic_id == -1)
                {
                    usleep(200000); // krótkie oczekiwanie
                    attempts++;
                }
            }

            if (mechanic_id != -1)
            {
                printf("    -> [Obsługa %d] Znalazłem wolne stanowisko %d. Przekazuje auto.\n", id + 1, mechanic_id + 1);

                msg.mtype = MSG_TYPE_MECHANIC_OFFSET + mechanic_id;
                if (msgsnd(msg_queue_id, &msg, sizeof(CarData), 0) == -1)
                {
                    perror("Błąd wysyłania auta do mechanika");
                    release_station(mechanic_id);
                }
            } else {
                printf("    -> [Obsługa %d] Brak wolnych stanowisk dla ID: %d. Klient rezygnuje z czekania.\n", id + 1, msg.data.id);
            }

            continue;
        }

        usleep(10000); // Małe opóźnienie, aby nie obciążać CPU
    }
    printf("[Obsługa %d] Zamykam stanowisko nr %d.\n", id + 1, id + 1);
    exit(0);
}