#include "common.h"

volatile int local_mech_id = -1;
volatile int speed_mode = 0; // 0 - normal, 1 - 50% szybciej
volatile int close_requested = 0;
volatile int job_in_progress = 0;

// Handler sygnałów
void signal_handler(int sig)
{
    if (sig == SIG_SPEED_UP)
    {
        if (speed_mode == 0)
        {
            speed_mode = 1; // Ignoruj kolejne próby
            log_color(ROLE_MECHANIC, "Mechanik %d: Zostalem przyśpieszony.", local_mech_id);
        }
    }
    else if (sig == SIG_RESTORE_SPEED)
    {
        if (speed_mode == 1)
        {
            speed_mode = 0; // Tylko jeśli był przyspieszony
            log_color(ROLE_MECHANIC, "Mechanik %d: Zostałem przywrócony do normalnego tempa.", local_mech_id);
        }
    }
    else if (sig == SIG_CLOSE_STATION)
    {
        close_requested = 1; // Zamknij po zakończeniu obecnego zadania
        log_color(ROLE_MECHANIC, "Mechanik %d: Otrzymałem polecenie zamknięcia stanowiska.", local_mech_id)
    }
    else if (sig == SIG_FIRE)
    {
        exit(0); // Natychmiastowe wyjście
    }
}

void run_mechanic(int mech_id, int msg_id, int shm_id, int sem_id)
{
    local_mech_id = mech_id;
    global_sem_id = sem_id;
    shared_data *shm = (shared_data *)shmat(shm_id, NULL, 0);

    // Rejestracja PID w pamięci dzielonej
    shm->mechanics_pids[mech_id - 1] = getpid();
    shm->mechanic_status[mech_id - 1] = 0; // Wolny

    // Konfiguracja obsługi sygnałów
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_SPEED_UP, &sa, NULL);
    sigaction(SIG_RESTORE_SPEED, &sa, NULL);
    sigaction(SIG_CLOSE_STATION, &sa, NULL);
    sigaction(SIG_FIRE, &sa, NULL);

    log_color(ROLE_MECHANIC, "Mechanik %d: Rozpoczynam pracę.", mech_id);

    msg_buf msg;
    while (shm->simulation_running)
    {
        if (close_requested && !job_in_progress)
        {
            shm->mechanic_status[mech_id - 1] = 2; // Zamknięte
            log_color(ROLE_MECHANIC, "Mechanik %d: Stanowisko zamknięte decyzją kierownika.", mech_id);
            break;
        }

        // Odbieranie zlecenia
        safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_TO_MECHANIC + mech_id);

        job_in_progress = 1;
        shm->mechanic_status[mech_id - 1] = 1; // Zajęty
        log_color(ROLE_MECHANIC, "Mechanik %d: Rozpoczynam naprawę auta marki %c (Klient PID: %d). Czas: %d",
                   mech_id, msg.brand, msg.client_pid, msg.duration);

        // Symulacja naprawy - Część 1 (50% czasu)
        int part1 = msg.duration * 1000000 / 2;
        if (speed_mode)
            part1 /= 2;
        custom_usleep(part1 > 0 ? part1 : 1);

        // Losowanie dodatkowej usterki (20% szans)
        if ((rand() % 100) < CHANCE_EXTRA_REPAIR)
        {
            log_color(ROLE_MECHANIC, "Mechanik %d: Znaleziono dodatkową usterkę (Klient PID: %d). Pytam o zgodę.", mech_id, msg.client_pid);

            // Pytanie do obsługi, następnie obsługa kontaktuje się z klientem
            msg.mtype = MSG_MECHANIC_TO_SERVICE;
            msg.is_extra_repair = 1;
            msg.mechanic_id = mech_id;
            safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

            // Czekanie na decyzję
            safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_TO_MECHANIC + mech_id);

            if (msg.decision == 1)
            {
                log_color(ROLE_MECHANIC, "Mechanik %d: Klient zgodził się na naprawę. Kontynuuję.", mech_id);
                // Dodatkowy czas (np. +20%)
                msg.duration += (msg.duration * 0.2);
            }
            else
            {
                log_color(ROLE_MECHANIC, "Mechanik %d: Klient odmówił dodatkowej naprawy.", mech_id);
            }
        }

        // Symulacja naprawy - Część 2
        int part2 = msg.duration * 1000000 - part1; // Reszta czasu
        if (speed_mode)
            part2 /= 2;
        custom_usleep(part2 > 0 ? part2 : 1);

        // Koniec naprawy
        log_color(ROLE_MECHANIC, "Mechanik %d: Zakończono naprawę (Klient PID: %d). Przekazuję do obsługi.", mech_id, msg.client_pid);
        msg.mtype = MSG_MECHANIC_TO_SERVICE;
        msg.is_extra_repair = 0; // Finalizacja
        msg.mechanic_id = mech_id;
        safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

        job_in_progress = 0;
        shm->mechanic_status[mech_id - 1] = 0; // Wolny

        if (close_requested)
        {
            shm->mechanic_status[mech_id - 1] = 2;
            log_color(ROLE_MECHANIC, "Mechanik %d: Zamykam stanowisko po zleceniu.", mech_id);
            break;
        }
    }
    shmdt(shm);
    exit(0);
}