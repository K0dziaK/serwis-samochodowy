#include "common.h"
#include "roles.h"

static pid_t local_service_pids[3] = {0};

void run_manager()
{
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