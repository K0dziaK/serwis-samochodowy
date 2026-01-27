#include "common.h"

// Zmienne globalne dla obsługi sygnałów
volatile int local_mech_id = -1;
volatile int speed_mode = 0;       // 0 - normalna prędkość, 1 - przyspieszony (50% szybciej)
volatile int close_requested = 0; // Flaga zamknięcia stanowiska
volatile int job_in_progress = 0; // Flaga trwającej naprawy

// Handler sygnałów sterujących pracą mechanika
void signal_handler(int sig)
{
    if (sig == SIG_SPEED_UP)
    {
        // Przyspieszenie pracy mechanika
        if (speed_mode == 0)
        {
            speed_mode = 1;
            log_color(ROLE_MECHANIC, "Mechanik %d: Zostalem przyśpieszony.", local_mech_id);
        }
    }
    else if (sig == SIG_RESTORE_SPEED)
    {
        // Przywrócenie normalnego tempa pracy
        if (speed_mode == 1)
        {
            speed_mode = 0;
            log_color(ROLE_MECHANIC, "Mechanik %d: Zostałem przywrócony do normalnego tempa.", local_mech_id);
        }
    }
    else if (sig == SIG_CLOSE_STATION)
    {
        // Żądanie zamknięcia stanowiska (po zakończeniu bieżącej naprawy)
        close_requested = 1;
        log_color(ROLE_MECHANIC, "Mechanik %d: Otrzymałem polecenie zamknięcia stanowiska.", local_mech_id);
    }
    else if (sig == SIG_FIRE)
    {
        // Natychmiastowe zamknięcie (pożar/ewakuacja)
        exit(0);
    }
}

// Główna funkcja pracy mechanika
void run_mechanic(int mech_id, int msg_id, int shm_id, int sem_id)
{
    local_mech_id = mech_id;
    global_sem_id = sem_id;
    shared_data *shm = (shared_data *)shmat(shm_id, NULL, 0);

    // Rejestracja PID mechanika w pamięci dzielonej
    shm->mechanics_pids[mech_id - 1] = getpid();
    shm->mechanic_status[mech_id - 1] = 0;

    // Konfiguracja handlerów sygnałów
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
    // Główna pętla pracy
    while (shm->simulation_running)
    {
        // Sprawdzanie czy zamknięcie stanowiska i brak aktywnej naprawy
        if (close_requested && !job_in_progress)
        {
            shm->mechanic_status[mech_id - 1] = 2;
            log_color(ROLE_MECHANIC, "Mechanik %d: Stanowisko zamknięte decyzją kierownika.", mech_id);
            break;
        }

        // Czekanie na zlecenie od obsługi
        safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_TO_MECHANIC + mech_id);

        // Rozpoczęcie naprawy
        job_in_progress = 1;
        // Status już ustawiony przez obsługę, ale potwierdzamy
        shm->mechanic_status[mech_id - 1] = 1;
        log_color(ROLE_MECHANIC, "Mechanik %d: Rozpoczynam naprawę auta marki %c (Klient PID: %d). Czas: %d",
                   mech_id, msg.brand, msg.client_pid, msg.duration);

        // Symulacja naprawy - część 1 (pierwsza połowa czasu)
        int part1 = msg.duration * 1000000 / 2;
        if (speed_mode)
            part1 /= 2;
        custom_usleep(part1 > 0 ? part1 : 1);

        // Losowanie dodatkowej usterki (szansa określona w common.h)
        if ((rand() % 100) < CHANCE_EXTRA_REPAIR)
        {
            log_color(ROLE_MECHANIC, "Mechanik %d: Znaleziono dodatkową usterkę (Klient PID: %d). Pytam o zgodę.", mech_id, msg.client_pid);

            // Wysyłanie pytania do obsługi (ta skontaktuje się z klientem)
            msg.mtype = MSG_MECHANIC_TO_SERVICE;
            msg.is_extra_repair = 1;
            msg.mechanic_id = mech_id;
            safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

            // Czekanie na decyzję klienta
            safe_msgrcv_wait(msg_id, &msg, sizeof(msg_buf) - sizeof(long), MSG_BASE_TO_MECHANIC + mech_id);

            if (msg.decision == 1)
            {
                log_color(ROLE_MECHANIC, "Mechanik %d: Klient zgodził się na naprawę. Kontynuuję.", mech_id);
                // Dodatkowy czas na naprawę usterki (+20%)
                msg.duration += (msg.duration * 0.2);
            }
            else
            {
                log_color(ROLE_MECHANIC, "Mechanik %d: Klient odmówił dodatkowej naprawy.", mech_id);
            }
        }

        // Symulacja naprawy - część 2 (druga połowa czasu)
        int part2 = msg.duration * 1000000 - part1;
        if (speed_mode)
            part2 /= 2;
        custom_usleep(part2 > 0 ? part2 : 1);

        // Zakończenie naprawy
        log_color(ROLE_MECHANIC, "Mechanik %d: Zakończono naprawę (Klient PID: %d). Przekazuję do obsługi.", mech_id, msg.client_pid);
        msg.mtype = MSG_MECHANIC_TO_SERVICE;
        msg.is_extra_repair = 0;
        msg.mechanic_id = mech_id;
        safe_msgsnd(msg_id, &msg, sizeof(msg_buf) - sizeof(long), 0);

        // Ustawienie statusu na wolny
        job_in_progress = 0;
        shm->mechanic_status[mech_id - 1] = 0;

        // Sprawdzanie żądania zamknięcia po zakończeniu naprawy
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