#include "common.h"
#include "ipc_utils.h"
#include "roles.h"

volatile sig_atomic_t speed_mode = 0;   // 0 - normalny, 1 - szybki
volatile sig_atomic_t closing_mode = 0; // 1 = zamknij po obecnym aucie

void mechanic_signal_handler(int sig)
{
    if (sig == SIG_SPEED_UP)
    {
        if (speed_mode == 0)
        {
            speed_mode = 1;
            char buff[] = "\n[SIGNAL] Otrzymano rozkaz przyśpieszenia pracy!\n";
            write(STDOUT_FILENO, buff, sizeof(buff) - 1);
        }
        else
        {
            // ignoruj
        }
    }
    else if (sig == SIG_NORMAL_SPEED)
    {
        if (speed_mode == 1)
        {
            speed_mode = 0;
            char buff[] = "\n[SIGNAL] Powrót do normalnego tempa.\n";
            write(STDOUT_FILENO, buff, sizeof(buff) - 1);
        }
        else
        {
            // ignoruj
        }
    }
    else if (sig == SIG_CLOSE_STATION)
    {
        closing_mode = 1;
        char buff[] = "\n[SIGNAL] Rozkaz zamknięcia stanowiska po obecnej naprawie.\n";
        write(STDOUT_FILENO, buff, sizeof(buff) - 1);
    }
    else if (sig == SIG_FIRE)
    {
        char buff[] = "\n[SIGNAL] POŻAR! UCIEKAM!!\n";
        write(STDERR_FILENO, buff, sizeof(buff) - 1);
        _exit(0);
    }
}

void run_mechanic(int id)
{
    CarMessage msg;
    long my_msg_type = MSG_TYPE_MECHANIC_OFFSET + id;
    srand(time(NULL) ^ getpid());

    // Rejestracja sygnałów
    struct sigaction sa;
    sa.sa_handler = mechanic_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIG_SPEED_UP, &sa, NULL);
    sigaction(SIG_NORMAL_SPEED, &sa, NULL);
    sigaction(SIG_CLOSE_STATION, &sa, NULL);
    sigaction(SIG_FIRE, &sa, NULL);

    printf("[Mechanik %d] Obsługuje stanowisko %d.\n", getpid(), id);

    while (1)
    {
        if (closing_mode)
        {
            printf("[Mechanik %d] Stanowisko zamknięte przez Kierownika. Kończę pracę.\n", id + 1);
            break;
        }

        if (msgrcv(msg_queue_id, &msg, sizeof(CarData), my_msg_type, 0) == -1)
        {
            if (errno == EINTR)
                continue;
            if (errno == EIDRM)
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

        int total_time = msg.data.time_est;
        if(speed_mode) {
            total_time /= 2;
            if(total_time < 1) total_time = 1;
            printf("    -> [Mechanik %d] Wykonuje przyśpieszoną naprawę!", id + 1);
        }

        int time_left = total_time;
        while (time_left > 0)
        {
            time_left = sleep(time_left);
        }

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
                        sleep(extra_time);
                    }
                    else
                    {
                        printf("    -> [Mechanik %d] Klient odrzucił dopłatę. Naprawiam tylko usterkę podstawową.\n", id + 1);
                    }
                }
            }
        }

        printf("    -> [Mechanik %d] Koniec naprawy auta ID: %d.\n", id + 1, msg.data.id);

        release_station(id);

        // Wysłanie informacji do kasy
        msg.mtype = MSG_TYPE_BILLING;
        msgsnd(msg_queue_id, &msg, sizeof(CarData), 0);
    }
    
    exit(0);
}