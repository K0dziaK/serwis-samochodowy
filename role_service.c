#include "common.h"

// Flaga zakończenia pracy stanowiska (ustawiana przez SIGTERM od menedżera)
static volatile int service_should_exit = 0;

// Handler sygnału SIGTERM dla stanowisk 2 i 3
static void service_sigterm_handler(int sig)
{
    (void)sig;
    service_should_exit = 1;
}

// Handler SIGCONT - wznowienie po zatrzymaniu
// Pozwala natychmiast sprawdzić flagi po wznowieniu procesu
static void service_sigcont_handler(int sig)
{
    (void)sig;
    // Pusty handler - samo przerwanie wystarczy, żeby przerwać blokujące operacje
}

// Obsługa klienta - stanowisko przyjmujące zgłoszenia i koordynujące naprawy
void run_service(int staff_id, int msg_id, int shm_id, int sem_id)
{
    global_sem_id = sem_id;
    shared_data *shm = (shared_data *)safe_shmat(shm_id, NULL, 0);
    msg_buf msg;
    
    // Kopiowanie czasu startu symulacji
    time_t start_time = shm->start_time;

    // Rejestracja handlerów sygnałów dla stanowisk dynamicznych (2 i 3)
    if (staff_id > 1)
    {
        signal(SIGTERM, service_sigterm_handler);
        signal(SIGCONT, service_sigcont_handler);
    }

    log_color(ROLE_SERVICE, "Obsługa %d: Meldunek na stanowisku.", staff_id);

    // Stan otwarcia: -1 = nieznany, 0 = zamknięte, 1 = otwarte
    int was_open = -1;
    
    // Flaga: czy klienci zostali już wyrzuceni po zamknięciu (resetowana przy otwarciu)
    int clients_dismissed_today = 1;

    // Główna pętla pracy stanowiska
    // Stanowisko 1 - stałe (działa do końca symulacji)
    // Stanowiska 2 i 3 - dynamiczne (zamykane przez menedżera)
    while (shm->simulation_running)
    {
        // Sprawdzenie flagi zakończenia dla stanowisk dynamicznych (2 i 3)
        if (staff_id > 1 && service_should_exit)
            break;

        // Obliczanie aktualnego czasu symulacji
        sim_time st = get_simulation_time(start_time);
        int is_open = (st.hour >= OPEN_HOUR && st.hour < CLOSE_HOUR);

        // Logika otwarcia/zamknięcia - tylko dla stanowiska 1 (główne)
        if (staff_id == 1)
        {
            // Przejście na zamknięte - wypychamy klientów TYLKO RAZ
            if (!is_open && !clients_dismissed_today)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Wybiła godzina %d:00. Zamykam przyjmowanie klientów.", staff_id, CLOSE_HOUR);
                
                // Wypychanie wszystkich oczekujących klientów z kolejki
                while (safe_msgrcv_nowait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_CLIENT_TO_SERVICE) != -1)
                {
                    log_color(ROLE_SERVICE, "Obsługa %d: (ZAMKNIĘCIE) Przepraszam klienta %d, zamykamy.", staff_id, msg.client_pid);
                    // Zwalnianie miejsca w kolejce
                    sem_lock(sem_id, SEM_LOG);
                    if (shm->clients_in_queue > 0)
                        shm->clients_in_queue--;
                    sem_unlock(sem_id, SEM_LOG);
                    sem_unlock(sem_id, SEM_QUEUE);
                    // Wysyłanie odmowy do klienta
                    msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
                    msg.decision = 0;
                    safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
                }
                
                // Oznaczamy, że klienci zostali już wyrzuceni
                clients_dismissed_today = 1;
            }
            // Przejście z zamkniętego na otwarte - początek dnia pracy
            else if (is_open && was_open == 0)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Godzina %d:00. Otwieram przyjmowanie klientów.", staff_id, OPEN_HOUR);
                // Resetujemy flagę na nowy dzień
                clients_dismissed_today = 0;
            }
            // Pierwsze uruchomienie w godzinach otwarcia
            else if (was_open == -1 && is_open)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Serwis otwarty. Rozpoczynam przyjmowanie klientów.", staff_id);
            }
        }
        
        // Zapamiętanie stanu otwarcia
        was_open = is_open ? 1 : 0;

        // Priorytetowa obsługa komunikatów od mechaników (zgłoszenia usterek lub zakończone naprawy)
        if (staff_id == 1 && safe_msgrcv_nowait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_MECHANIC_TO_SERVICE) != -1)
        {
            if (msg.is_extra_repair == 1)
            {
                // Mechanik znalazł dodatkową usterkę - pytanie klienta o zgodę
                log_color(ROLE_SERVICE, "Obsługa %d: Mechanik %d -> Klient %d (Usterka dod.).", staff_id, msg.mechanic_id, msg.client_pid);
                msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

                // Czekanie na decyzję klienta
                safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_CLIENT_DECISION + msg.client_pid);

                // Przekazanie decyzji do mechanika
                msg.mtype = MSG_BASE_TO_MECHANIC + msg.mechanic_id;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
            }
            else
            {
                // Naprawa zakończona - przekazanie klienta do kasy
                log_color(ROLE_SERVICE, "Obsługa %d: Auto klienta %d gotowe. Do Kasy.", staff_id, msg.client_pid);
                msg.mtype = MSG_SERVICE_TO_CASHIER;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
            }
            continue;
        }

        // Obsługa nowych klientów (tylko gdy serwis otwarty)
        if (is_open && safe_msgrcv_nowait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_CLIENT_TO_SERVICE) != -1)
        {
            // Zwalnianie miejsca w kolejce po przyjęciu klienta
            sem_lock(sem_id, SEM_LOG);
            if (shm->clients_in_queue > 0)
                shm->clients_in_queue--;
            sem_unlock(sem_id, SEM_LOG);
            
            sem_unlock(sem_id, SEM_QUEUE);

            // Pobranie nazwy usługi z cennika
            int sid = msg.service_id;
            if (sid < 0 || sid >= NUM_SERVICES)
                sid = 0;
            const char *s_name = PRICE_LIST[sid].name;

            log_color(ROLE_SERVICE, "Obsługa %d: Klient %d, Auto: %c, Zgłoszenie: %s", staff_id, msg.client_pid, msg.brand, s_name);

            // Weryfikacja czy marka jest obsługiwana (tylko A, E, I, O, U, Y)
            const char allowed[] = "AEIOUY";
            if (strchr(allowed, msg.brand) == NULL)
            {
                msg.decision = 0;
                msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;
                log_color(ROLE_SERVICE, "Obsługa %d: Marka %c nieobsługiwana.", staff_id, msg.brand);
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
                continue;
            }

            // Kalkulacja wyceny (+/- 10% od ceny bazowej)
            int base_cost = PRICE_LIST[sid].base_cost;
            int variance = (base_cost * 10) / 100;
            msg.cost = base_cost + (rand() % (variance * 2 + 1)) - variance;
            msg.duration = PRICE_LIST[sid].base_duration;
            // Losowa modyfikacja czasu dla dłuższych napraw
            if (msg.duration > 2)
                msg.duration += (rand() % 3) - 1;

            msg.decision = 1;
            msg.mtype = MSG_BASE_TO_CLIENT + msg.client_pid;

            log_color(ROLE_SERVICE, "Obsługa %d: Oferta dla klienta %d: %d PLN, czas %d.", staff_id, msg.client_pid, msg.cost, msg.duration);
            safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

            // Czekanie na decyzję klienta (akceptacja lub odmowa)
            safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_CLIENT_DECISION + msg.client_pid);

            if (msg.decision == 0)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Klient %d odrzucił koszty.", staff_id, msg.client_pid);
                continue;
            }

            // Szukanie wolnego mechanika (od końca, mechanik 8 tylko dla marek U/Y)
            // Sekcja krytyczna - tylko jeden wątek może przypisywać mechanika na raz
            sem_lock(sem_id, SEM_MECHANIC_ASSIGN);
            
            int found = -1;
            for (int i = NUM_MECHANICS - 1; i >= 0; i--)
            {
                // Sprawdzenie czy mechanik jest wolny (status 0)
                if (shm->mechanic_status[i] == 0)
                {
                    int is_uy = (msg.brand == 'U' || msg.brand == 'Y');
                    // Mechanik 8 (index 7) - specjalista od marek U i Y
                    if (i == 7)
                    {
                        if (is_uy)
                        {
                            found = i + 1;
                            // Natychmiast oznaczamy jako zajętego, żeby inny wątek nie wysłał do niego zlecenia
                            shm->mechanic_status[i] = 1;
                            break;
                        }
                    }
                    else
                    {
                        found = i + 1;
                        // Natychmiast oznaczamy jako zajętego, żeby inny wątek nie wysłał do niego zlecenia
                        shm->mechanic_status[i] = 1;
                        break;
                    }
                }
            }
            
            sem_unlock(sem_id, SEM_MECHANIC_ASSIGN);

            // Przydzielanie mechanika lub odprawianie klienta
            if (found != -1)
            {
                log_color(ROLE_SERVICE, "Obsługa %d: Przypisano mechanika %d.", staff_id, found);
                msg.mtype = MSG_BASE_TO_MECHANIC + found;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
            }
            else
            {
                // Brak wolnych mechaników - klient odprawiony bez naprawy
                log_color(ROLE_SERVICE, "Obsługa %d: BRAK WOLNYCH MIEJSC. Odprawiam klienta %d.", staff_id, msg.client_pid);
                msg.cost = 0;
                msg.is_extra_repair = 0;
                msg.mtype = MSG_SERVICE_TO_CASHIER;
                safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);
            }
        }

        // Krótkie opóźnienie żeby nie obciążać procesora
        custom_usleep(50000);
    }

    // Zamykanie stanowiska
    if (staff_id > 1)
    {
        log_color(ROLE_SERVICE, "Obsługa %d: Zamykam stanowisko na polecenie menedżera.", staff_id);
    }

    shmdt(shm);
    exit(0);
}