#include "common.h"

void run_mechanic(int id) {
    printf("[Mechanik %d] Obsługuje stanowisko %d.\n", getpid(), id);
    if(id == 7) {
        printf("[Mechanik %d] Obsługuje tylko marki U i Y.\n", getpid()); // z treści zadania
    }
    sleep(1);
    exit(0);
}

void run_service_worker(int id) {
    printf("[Obsługa %d] Obsługuje stanowisko obsługi %d.\n", getpid(), id);
    sleep(1);
    exit(0);
}

void run_manager() {
    printf("[Kierownik %d] Nadzoruję pracę serwisu.\n", getpid());
    sleep(1);
    exit(0);
}

int main() {
    pid_t pid;

    printf("=== Rozpoczęcie symulacji ===\n");

    pid = fork();
    if(pid == 0) {
        run_manager();
    }else if(pid < 0) {
        perror("Błąd: nie powiodło się uruchomienie kierownika.");
        exit(1);
    }
    for (int i = 0; i < NUM_MECHANICS; i++) {
        pid = fork();
        if(pid == 0){
            run_mechanic(i);
        }else if (pid < 0){
            perror("Błąd: nie powiodło się uruchomienie mechanika.");
            exit(1);
        }
    }

    for (int i = 0; i < MIN_SERVICE_DESKS; i++) {
        pid = fork();
        if(pid == 0) {
            run_service_worker(i);
        }else if(pid < 0) {
            perror("Błąd: nie powiodło się uruchamianie obsługi.");
            exit(1);
        }
    }

    while(wait(NULL) > 0);

    printf("=== Koniec symulacji ===\n");
    return 0;
}