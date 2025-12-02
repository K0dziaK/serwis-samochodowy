#include "common.h"

int msg_queue_id;
pid_t service_pids[3] = {0};

int is_supported_brand(char brand)
{
    for (int i = 0; i < NUM_ALLOWED_BRANDS; i++)
    {
        if (ALLOWED_BRANDS[i] == brand)
            return 1;
    }
    return 0;
}

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
            if (errno == EIDRM)
                break;
        }

        usleep((rand() % 1000000) + 100000);
    }
    exit(0);
}

void run_mechanic(int id)
{
    printf("[Mechanik %d] Obsługuje stanowisko %d.\n", getpid(), id);
    if (id == 7)
    {
        printf("[Mechanik %d] Obsługuje tylko marki U i Y.\n", getpid()); // z treści zadania
    }
    while (1)
    {
        sleep(10);
    }
}

void run_service_worker(int id)
{
    CarMessage msg;
    printf("[Obsługa %d] Obsługuje stanowisko obsługi %d.\n", getpid(), id);

    while (1)
    {
        if (msgrcv(msg_queue_id, &msg, sizeof(CarData), MSG_TYPE_NEW_CLIENT, 0) == -1)
        {
            if (errno = EIDRM || errno == EINTR)
                break;
            printf("[Obsługa %d] Błąd msgrcv", getpid());
            exit(1);
        }

        printf("[Obsługa %d] Podszedł klient ID: %d, Auto: %c.\n", getpid(), msg.data.id, msg.data.brand);

        if (is_supported_brand(msg.data.brand))
        {
            printf("    -> [Obsługa %d] Marka %c zaakceptowana. Wycena naprawy...\n", getpid(), msg.data.brand);
            // symulacja wyceny
            sleep(2);
        }
        else
        {
            printf("    -> [Obsługa %d] Marka %c odrzucona. Klient odchodzi.\n", getpid(), msg.data.brand);
            sleep(1);
        }
    }
    printf("[Obsługa %d] Zamykam stanowisko nr %d.\n", getpid(), id + 1);
    exit(0);
}

void run_manager()
{
    struct msqid_ds buf;
    printf("[Kierownik %d] Rozpoczynam monitorowanie kolejki.\n", getpid());
    while (1)
    {
        if (msgctl(msg_queue_id, IPC_STAT, &buf) == -1)
        {
            if (errno == EIDRM)
                break;
            printf("[Kierownik %d] Błąd msgctl", getpid());
        }
        else
        {
            int qnum = buf.msg_qnum;
            printf("[Kierownik %d] Rozmiar kolejki %d.\n", getpid(), qnum);

            if (qnum > 3 && service_pids[1] == 0)
            {
                printf("[Kierownik %d] Kolejka rośnie (%d osób). Otwieram stanowisko obsługi nr. 2\n", getpid(), qnum);
                pid_t p = fork();
                if (p == 0)
                    run_service_worker(1);
                else if (p > 0)
                    service_pids[1] = p;
            }
            else if (qnum <= 2 && service_pids[1] != 0)
            {
                printf("[Kierownik %d] Kolejka maleje (%d osób). Zamykam stanowisko obsługi nr. 2\n", getpid(), qnum);
                kill(service_pids[1], SIGTERM);
                waitpid(service_pids[1], NULL, 0);
                service_pids[1] = 0;
            }

            if (qnum > 5 && service_pids[2] == 0)
            {
                printf("[Kierownik %d] Kolejka rośnie (%d osób). Otwieram stanowisko obsługi nr. 3\n", getpid(), qnum);
                pid_t p = fork();
                if (p == 0)
                    run_service_worker(2);
                else if (p > 0)
                    service_pids[2] = p;
            }
            else if (qnum <= 3 && service_pids[2] != 0)
            {
                printf("[Kierownik %d] Kolejka maleje (%d osób). Zamykam stanowisko obsługi nr. 3\n", getpid(), qnum);
                kill(service_pids[2], SIGTERM);
                waitpid(service_pids[2], NULL, 0);
                service_pids[2] = 0;
            }
        }
        sleep(1);
    }
}

int main()
{
    pid_t pid;

    printf("=== Inicjalizacja IPC ===\n");

    msg_queue_id = msgget(QUEUE_KEY, IPC_CREAT | 0666);
    if (msg_queue_id == -1)
    {
        perror("Błąd: nie powiodło się tworzenie kolejki.");
        exit(1);
    }
    printf("[System] Utworzono kolejkę komunikatów o id: %d\n", msg_queue_id);

    printf("=== Rozpoczęcie symulacji ===\n");

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

    sleep(20);

    printf("\n=== Koniec symulacji ===\n");

    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
    {
        perror("Błąd: nie powiodło się usuwanie kolejki.");
    }
    else
    {
        printf("[System] Usunięto kolejkę komunikatów.\n");
    }

    kill(0, SIGTERM); // na razie do testów, ubicie procesów w ten sposób

    return 0;
}