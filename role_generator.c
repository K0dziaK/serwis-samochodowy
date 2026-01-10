#include "common.h"
#include "roles.h"
#include "ipc_utils.h"

void run_client_generator()
{
    srand(time(NULL) ^ getpid());
    int client_id_counter = 0;
    SharedTime *time_ptr = attach_shared_time();

    if (!time_ptr)
    {
        fprintf(stderr, "Generator nie widzi zegara!\n");
        exit(1);
    }

    printf("[Generator] Rozpoczynam generowanie klientów.\n");

    while (1)
    {
        int hour = time_ptr->current_hour;
        int is_open = (hour >= TP && hour < TK);

        char brand = 'A' + (rand() % 26);
        int service_id = rand() % NUM_SERVICES;

        CarMessage msg;
        msg.mtype = MSG_TYPE_NEW_CLIENT;
        msg.data.id = client_id_counter++;
        msg.data.brand = brand;
        msg.data.is_vip = (rand() % 100 < VIP_CUSTOMER_CHANCE);

        strcpy(msg.data.service_name, SERVICE_LIST[service_id].name);
        msg.data.cost = SERVICE_LIST[service_id].base_cost;
        msg.data.time_est = SERVICE_LIST[service_id].base_time;
        msg.data.is_critical_fault = SERVICE_LIST[service_id].is_critical;

        int will_wait = 0;
        if (is_open)
        {
            will_wait = 1; // Warsztat otwarty, więc poczeka.
        }
        else
        {
            // Warsztat zamknięty. Sprawdzamy czy klient poczeka.

            int hours_to_open = 0;
            if (hour < TP)
                hours_to_open = TP - hour;
            else
                hours_to_open = (24 - hour) + TP;

            if (msg.data.is_critical_fault)
            {
                printf("[Klient %d] Usterka krytyczna, poczekam mimo zamknięcia.\n", msg.data.id);
                will_wait = 1;
            }
            else if (hours_to_open <= T1)
            {
                printf("[Klient %d] Do otwarcia tylko %dh, poczekam do otwarcia.\n", msg.data.id, hours_to_open);
                will_wait = 1;
            }
            else
            {
                printf("[Klient %d] Do otwarcia jeszcze %dh, rezygnuję.\n", msg.data.id, hours_to_open);
                will_wait = 0;
            }
        }

        if (will_wait)
        {
            if (msgsnd(msg_queue_id, &msg, sizeof(CarData), 0) == -1)
            {
                if (errno == EIDRM)
                    break;
            }
        }

        // Losowy czas przyjścia kolejnego klienta
        usleep((rand() % 1500000) + 500000);
    }
    shmdt(time_ptr);
    exit(0);
}