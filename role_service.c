#include "common.h"

// Flaga dla sygnału SIGTERM (dla stanowisk 2 i 3 zarządzanych przez menedżera)
static volatile int service_should_exit = 0;

static void service_sigterm_handler(int sig)
{
    (void)sig;
    service_should_exit = 1;
}

void run_service(int staff_id, int msg_id, int shm_id, int sem_id)
{
    global_sem_id = sem_id;
    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);
    msg_buf msg;
    
    // Zapamiętanie start_time
    time_t start_time = shm->start_time;

    // Stanowiska 2 i 3 reagują na SIGTERM od menedżera
    if (staff_id > 1)
    {
        signal(SIGTERM, service_sigterm_handler);
    }

    log_color(ROLE_SERVICE, "Obsługa %d: Meldunek na stanowisku.", staff_id);

    int was_open = -1;  // -1 = nieznany stan początkowy, 0 = zamknięte, 1 = otwarte

    // Stanowisko 1 działa dopóki symulacja trwa
    // Stanowiska 2 i 3 działają dopóki menedżer ich nie zamknie (SIGTERM)
    while (shm->simulation_running && (staff_id == 1 || !service_should_exit))
    {
        // Obliczenie aktualnego czasu symulacji
        sim_time st = get_simulation_time(start_time);
        int is_open = (st.hour >= OPEN_HOUR && st.hour < CLOSE_HOUR);

        // Logika otwarcia/zamknięcia stanowiska
        if (staff_id == 1)
        {
            // Przejście z otwartego na zamknięte
            if (was_open == 1 && !is_open)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Wybiła godzina %d:00. Zamykam przyjmowanie klientów.", staff_id, CLOSE_HOUR);
                
                // Wypchnijęcie wszystkich oczekujących klientów
                while (safe_msgrcv_nowait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_CLIENT_TO_SERVICE) != -1)
                {
                    log_color(ROLE_SERVICE, "Obsługa %d: (ZAMKNIĘCIE) Przepraszam klienta %d, zamykamy.", staff_id, msg.client_pid);
                    sem_lock(sem_id, SEM_LOG);
                    shm->clients_in_queue--;
                    sem_unlock(sem_id, SEM_LOG);
                    sem_unlock(sem_id, SEM_QUEUE);
                    msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
                    msg.decision = 0;
                    safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
                }
            }
            // Przejście z zamkniętego na otwarte
            else if (was_open == 0 && is_open)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Godzina %d:00. Otwieram przyjmowanie klientów.", staff_id, OPEN_HOUR);
            }
            // Pierwsze uruchomienie
            else if (was_open == -1 && is_open)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Serwis otwarty. Rozpoczynam przyjmowanie klientów.", staff_id);
            }
        }
        
        // Zapamiętanie stanu otwarcia
        was_open = is_open ? 1 : 0;

        // Priorytetowo obsługujemy mechaników
        if (staff_id == 1 && safe_msgrcv_nowait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_MECHANIC_TO_SERVICE) != -1)
        {
            if (msg.is_extra_repair == 1)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Mechanik %d -> Klient %d (Usterka dod.).", staff_id, msg.mechanic_id, msg.client_pid);
                msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

                // Czekanie na decyzję klienta
                safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_CLIENT_DECISION + msg.client_pid);

                msg.mtype = MSG_BASE_TO_MECHANIC + msg.mechanic_id;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
            }
            else
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Auto klienta %d gotowe. Do Kasy.", staff_id, msg.client_pid);
                msg.mtype = MSG_SERVICE_TO_CASHIER;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
            }
            continue;
        }

        // Obsługa klientów tylko jeśli stanowisko jest otwarte
        if (is_open && safe_msgrcv_nowait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_CLIENT_TO_SERVICE) != -1)
        {
            // Zwolnienie miejsca w kolejce i dekrementacja licznika
            sem_lock(sem_id, SEM_LOG);
            shm->clients_in_queue--;
            sem_unlock(sem_id, SEM_LOG);
            
            sem_unlock(sem_id, SEM_QUEUE);

            // Pobranie danych o usłudze
            int sid = msg.service_id;
            if (sid < 0 || sid >= NUM_SERVICES)
                sid = 0;
            const char *s_name = PRICE_LIST[sid].name;

            log_color(ROLE_SERVICE, "Obsługa %d: Klient %d, Auto: %c, Zgłoszenie: %s", staff_id, msg.client_pid, msg.brand, s_name);

            // Weryfikacja marki
            const char allowed[] = "AEIOUY";
            if (strchr(allowed, msg.brand) == NULL)
            {
                msg.decision = 0;
                msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
                log_color(ROLE_SERVICE, "Obsługa %d: Marka %c nieobsługiwana.", staff_id, msg.brand);
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
                continue;
            }

            // Wycena
            int base_cost = PRICE_LIST[sid].base_cost;
            int variance = (base_cost * 10) / 100;
            msg.cost = base_cost + (rand() % (variance * 2 + 1)) - variance;
            msg.duration = PRICE_LIST[sid].base_duration;
            if (msg.duration > 2)
                msg.duration += (rand() % 3) - 1;

            msg.decision = 1;
            msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;

            log_color(ROLE_SERVICE, "Obsługa %d: Oferta dla klienta %d: %d PLN, czas %d.", staff_id, msg.client_pid, msg.cost, msg.duration);
            safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

            // Czekanie na decyzję
            safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_CLIENT_DECISION + msg.client_pid);

            if (msg.decision == 0)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Klient %d odrzucił koszty.", staff_id, msg.client_pid);
                continue;
            }

            // Szukanie mechanika
            int found = -1;
            for (int i = NUM_MECHANICS - 1; i >= 0; i--)
            {
                // Sprawdzenie czy mechanik wolny
                if (shm->mechanic_status[i] == 0)
                {
                    int is_uy = (msg.brand == 'U' || msg.brand == 'Y');
                    if (i == 7)
                    {
                        if (is_uy)
                        {
                            found = i + 1;
                            break;
                        }
                    }
                    else
                    {
                        found = i + 1;
                        break;
                    }
                }
            }

            // Przydzielanie mechanika lub odsyłanie klienta
            if (found != -1)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Przypisano mechanika %d.", staff_id, found);
                msg.mtype = MSG_BASE_TO_MECHANIC + found;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
            }
            else
            {
                log_color(ROLE_SERVICE, "Obsługa %d: BRAK WOLNYCH MIEJSC. Odprawiam klienta %d.", staff_id, msg.client_pid);
                msg.cost = 0;
                msg.is_extra_repair = 0; // 0 oznacza odprawienie klienta
                msg.mtype = MSG_SERVICE_TO_CASHIER;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
            }
        }

        // Krótkie opóźnienie aby nie obciążać CPU
        custom_usleep(50000); // 50ms
    }

    // Przy wyjściu
    if (staff_id > 1)
    {
        log_color(ROLE_SERVICE, "Obsługa %d: Zamykam stanowisko na polecenie menedżera.", staff_id);
    }

    shmdt(shm);
    exit(0);
}