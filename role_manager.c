#include "common.h"
#include "roles.h"
#include "ipc_utils.h"
#include <pthread.h>

static pid_t local_service_pids[3] = {0};

void *command_thread(void *arg)
{
    (void)arg;
    SharedData *pids_data = attach_shared_pids();
    if (!pids_data)
        return NULL;

    printf("\n--- MENU KIEROWNIKA ---\n");
    printf("[1 ID] - Zamknij stanowisko ID (Sygnał 1)\n");
    printf("[2 ID] - Szybkie tempo stanowisko ID (Sygnał 2)\n");
    printf("[3 ID] - Normalne tempo stanowisko ID (Sygnał 3)\n");
    printf("[4] - Pożar! Ewakuacja wszystkich (Sygnał 4)\n");

    int cmd, target_id;
    while (1)
    {
        if (scanf("%d", &cmd) == 1)
        {
            if (cmd == 4)
            {
                printf("[Kierownik] Ogłaszam alarm pożarowy!\n");
                kill(0, SIG_FIRE);
                break;
            }

            if (scanf("%d", &target_id) == 1)
            {
                if (target_id < 1 || target_id > NUM_MECHANICS)
                {
                    printf("Niepoprawne ID mechanika (1-%d)", NUM_MECHANICS);
                    continue;
                }

                pid_t target_pid = pids_data->mechanic_pids[target_id - 1];
                if (target_pid == 0)
                {
                    printf("Mechanik %d nie pracuje.\n", target_pid);
                    continue;
                }

                switch (cmd)
                {
                case 1:
                    printf("[Kierownik] Zamykam stanowisko %d (PID %d)\n", target_id, target_pid);
                    kill(target_pid, SIG_CLOSE_STATION);
                    break;
                case 2:
                    printf("[Kierownik] Przyśpieszam stanowisko %d (PID %d)\n", target_id, target_pid);
                    kill(target_pid, SIG_SPEED_UP);
                    break;
                case 3:
                    printf("[Kierownik] Spowalniam stanowisko %d (PID %d)\n", target_id, target_pid);
                    kill(target_pid, SIG_NORMAL_SPEED);
                    break;

                default:
                    printf("Nieznana komenda.\n");
                    break;
                }
            }
        }
    }
    shmdt(pids_data);
    return NULL;
}

void run_manager()
{

    pthread_t tid;
    pthread_create(&tid, NULL, command_thread, NULL);
    pthread_detach(tid);

    struct msqid_ds buf;
    printf("[Kierownik %d] Rozpoczynam monitorowanie kolejki.\n", getpid());

    while (1)
    {
        if (msgctl(msg_queue_id, IPC_STAT, &buf) == -1)
        {
            if (errno == EIDRM) // Kolejka usunięta
                break;
            printf("[Kierownik %d] Błąd msgctl", getpid());
        }
        else
        {
            int qnum = buf.msg_qnum; // Liczba klientów w kolejce

            // Logika otwierania stanowiska nr 2
            if (qnum > 3 && local_service_pids[1] == 0)
            {
                printf("[Kierownik %d] Kolejka rośnie (%d osób). Otwieram stanowisko obsługi nr. 2\n", getpid(), qnum);
                pid_t p = fork();
                if (p == 0)
                    run_service_worker(1);
                else if (p > 0)
                    local_service_pids[1] = p;
            }
            else if (qnum <= 2 && local_service_pids[1] != 0)
            {
                printf("[Kierownik %d] Kolejka maleje (%d osób). Zamykam stanowisko obsługi nr. 2\n", getpid(), qnum);
                kill(local_service_pids[1], SIGTERM);
                waitpid(local_service_pids[1], NULL, 0);
                local_service_pids[1] = 0;
            }

            // Logika otwierania stanowiska nr 3
            if (qnum > 5 && local_service_pids[2] == 0)
            {
                printf("[Kierownik %d] Kolejka rośnie (%d osób). Otwieram stanowisko obsługi nr. 3\n", getpid(), qnum);
                pid_t p = fork();
                if (p == 0)
                    run_service_worker(2);
                else if (p > 0)
                    local_service_pids[2] = p;
            }
            else if (qnum <= 3 && local_service_pids[2] != 0)
            {
                printf("[Kierownik %d] Kolejka maleje (%d osób). Zamykam stanowisko obsługi nr. 3\n", getpid(), qnum);
                kill(local_service_pids[2], SIGTERM);
                waitpid(local_service_pids[2], NULL, 0);
                local_service_pids[2] = 0;
            }
        }
        sleep(1);
    }
}