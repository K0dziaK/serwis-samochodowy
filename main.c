#include "common.h"
#include "ipc_utils.h"
#include "roles.h"

void run_time_manager()
{
    SharedTime *time_ptr = attach_shared_time();
    if (!time_ptr)
        exit(1);

    printf("[ZEGAR] Start symulacji czasu. 1h = 2s rzeczywista.\n");
    while (1)
    {
        printf("\n=== GODZINA %02d:00 ===\n", time_ptr->current_hour);
        if (time_ptr->current_hour == TP)
            printf("[System] Warsztat został otwarty.\n");
        if (time_ptr->current_hour == TK)
            printf("[System] Warsztat został zamknięty.\n");

        sleep(2);

        time_ptr->current_hour++;
        if (time_ptr->current_hour >= 24)
            time_ptr->current_hour = 0;
    }
}

void start_time_manager()
{
    int pid = fork();
    if (pid == 0)
    {
        run_time_manager();
    }
    else if (pid < 0)
    {
        perror("Błąd: nie powiodło się uruchomienie zegara.");
        exit(1);
    }
}

void start_manager()
{
    int pid = fork();
    if (pid == 0)
    {
        run_manager();
    }
    else if (pid < 0)
    {
        perror("Błąd: nie powiodło się uruchomienie kierownika.");
        exit(1);
    }
}

void start_mechanics()
{
    SharedData* pids_data = attach_shared_pids();

    for (int i = 0; i < NUM_MECHANICS; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            run_mechanic(i);
        }
        else if (pid > 0)
        {
            pids_data->mechanic_pids[i] = pid;
        }
        else if (pid < 0)
        {
            perror("Błąd: nie powiodło się uruchomienie mechanika.");
            exit(1);
        }
    }

    shmdt(pids_data);
}

void start_generator()
{
    int pid = fork();
    if (pid == 0)
    {
        run_client_generator();
    }
    else if (pid < 0)
    {
        perror("Błąd: nie powiodło się uruchomienie generatora klientów.");
        exit(1);
    }
}

void start_service()
{
    int pid = fork();
    if (pid == 0)
    {
        run_service_worker(0);
    }
    else if (pid < 0)
    {
        perror("Błąd: nie powiodło się uruchamianie obsługi.");
        exit(1);
    }
}

int main()
{
    init_queue();
    init_semaphores();
    init_shared_time();
    init_shared_pids();

    printf("=== Rozpoczęcie symulacji ===\n");

    start_time_manager();
    start_manager();
    start_mechanics();
    start_service();
    start_generator();

    sleep(60);

    printf("\n=== Koniec symulacji ===\n");

    clean_queue();
    clean_semaphores();
    remove_shared_time();
    remove_shared_pids();

    // Zabicie wszystkich procesów potomnych
    kill(0, SIGTERM);

    return 0;
}