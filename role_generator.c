#include "common.h"
#include "roles.h"

void run_client_generator()
{
    srand(time(NULL) ^ getpid());
    int client_id_counter = 0;

    printf("[Generator %d] Rozpoczynam generowanie klientów.\n", getpid());

    while (1)
    {
        char brand = 'A' + (rand() % 26);

        CarMessage msg;
        msg.mtype = MSG_TYPE_NEW_CLIENT;
        msg.data.id = client_id_counter++;
        msg.data.brand = brand;
        msg.data.is_vip = (rand() % 100 < VIP_CUSTOMER_CHANCE);

        if (msgsnd(msg_queue_id, &msg, sizeof(CarData), 0) == -1)
        {
            if (errno == EIDRM) // Kolejka usunięta - koniec pracy
                break;
        }

        // Losowy czas przyjścia kolejnego klienta
        usleep((rand() % 1500000) + 500000);
    }
    exit(0);
}