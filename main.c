#include "common.h"
#include "ipc_utils.h"
#include "roles.h"

int main()
{
    pid_t pid;

    printf("=== Inicjalizacja IPC ===\n");

    // Tworzenie kolejki
    msg_queue_id = msgget(QUEUE_KEY, IPC_CREAT | 0666);
    if (msg_queue_id == -1)
    {
        perror("Błąd: nie powiodło się tworzenie kolejki.");
        exit(1);
    }
    printf("[System] Utworzono kolejkę komunikatów o id: %d\n", msg_queue_id);

    // Tworzenie i inicjalizacja semaforów
    init_semaphores();

    printf("=== Rozpoczęcie symulacji ===\n");

    // 1. Uruchomienie Kierownika
    pid = fork();
    if (pid == 0)
    {
        run_manager();
    }
    else if (pid < 0)
    {
        perror("Błąd: nie powiodło się uruchomienie kierownika.");
        exit(1);
    }

    // 2. Uruchomienie Mechaników (8 stanowisk)
    for (int i = 0; i < NUM_MECHANICS; i++)
    {
        pid = fork();
        if (pid == 0)
        {
            run_mechanic(i);
        }
        else if (pid < 0)
        {
            perror("Błąd: nie powiodło się uruchomienie mechanika.");
            exit(1);
        }
    }

    // 3. Uruchomienie pierwszego pracownika obsługi (Kierownik zarządza resztą)
    pid = fork();
    if (pid == 0)
    {
        run_service_worker(0);
    }
    else if (pid < 0)
    {
        perror("Błąd: nie powiodło się uruchamianie obsługi.");
        exit(1);
    }

    // 4. Uruchomienie Generatora Klientów
    pid = fork();
    if (pid == 0)
    {
        run_client_generator();
    }
    else if (pid < 0)
    {
        perror("Błąd: nie powiodło się uruchomienie generatora klientów.");
        exit(1);
    }

    // Symulacja działa przez 60 sekund
    sleep(60);

    printf("\n=== Koniec symulacji ===\n");

    // Sprzątanie IPC
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
    {
        perror("Błąd: nie powiodło się usuwanie kolejki.");
    }
    else
    {
        printf("[System] Usunięto kolejkę komunikatów.\n");
    }

    if (semctl(sem_id, 0, IPC_RMID) == -1)
    {
        perror("Błąd: nie powiodło się usuwanie semaforów");
    }
    else
    {
        printf("[System] Usunięto zbiór semaforów.\n");
    }

    // Zabicie wszystkich procesów potomnych
    kill(0, SIGTERM);

    return 0;
}