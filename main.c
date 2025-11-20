#include "common.h"

int msg_queue_id;

int is_supported_brand(char brand) {
    for(int i = 0; i < NUM_ALLOWED_BRANDS; i++) {
        if(ALLOWED_BRANDS[i] == brand) return 1;
    }
    return 0;
}

void run_client_generator() {
    srand(time(NULL) ^ getpid());
    int client_id_counter = 0;

    printf("[Generator %d] Rozpoczynam generowanie klientów.\n", getpid());

    while(1) {
        char brand = 'A' + (rand() % 26);

        if(is_supported_brand(brand)) {
            CarMessage msg;
            msg.mtype = MSG_TYPE_NEW_CLIENT;
            msg.data.id = client_id_counter++;
            msg.data.brand = brand;
            msg.data.is_vip = (rand() % 100 < 2);

            if(msgsnd(msg_queue_id, &msg, sizeof(CarData), 0) == -1) {
                if(errno == EIDRM) break;
                printf("[Generator %d] Błąd msgsnd\n", getpid());
            } else {
                printf("[Generator %d] Nowy klient o id %d (Auto: %c) wszedł do serwisu.\n", getpid(), msg.data.id, msg.data.brand);
            }
        } else {
            printf("[Generator %d] Nowy klient o id %d (Auto: %c) wszedł do serwisu, ale nie obsługujemy jego marki samochodu.\n", getpid(), client_id_counter++, brand);
        }

        usleep((rand() % 2000000) + 100000);
    }
    exit(0);
}

void run_mechanic(int id) {
    printf("[Mechanik %d] Obsługuje stanowisko %d.\n", getpid(), id);
    if(id == 7) {
        printf("[Mechanik %d] Obsługuje tylko marki U i Y.\n", getpid()); // z treści zadania
    }
    while(1) {
        sleep(10);
    }
}

void run_service_worker(int id) {
    CarMessage msg;
    printf("[Obsługa %d] Obsługuje stanowisko obsługi %d.\n", getpid(), id);
    
    while(1) {
        if(msgrcv(msg_queue_id, &msg, sizeof(CarData), MSG_TYPE_NEW_CLIENT, 0) == -1){
            if(errno = EIDRM) break;
            printf("[Obsługa %d] Błąd msgrcv", getpid());
            exit(1);
        }

        printf("[Obsługa %d] Przyjąłem klienta ID: %d, Auto: %c.\n", getpid(), msg.data.id, msg.data.brand);
    
        sleep(1);
    }
    exit(0);
}

void run_manager() {
    printf("[Kierownik %d] Nadzoruję pracę serwisu.\n", getpid());
    while(1){
        sleep(10);
    }
}

void test_client_generator() {
    srand(time(NULL));
    for(int i = 0; i < 5; i++){
        CarMessage msg;
        msg.mtype = MSG_TYPE_NEW_CLIENT;
        msg.data.id = i + 100;
        msg.data.brand = ALLOWED_BRANDS[rand() % NUM_ALLOWED_BRANDS];

        if(msgsnd(msg_queue_id, &msg, sizeof(CarData), 0) == -1) {
            perror("Błąd: nie powiodło się wysłanie testowego klienta.");
        } else {
            printf("[Generator] KLient wszedł do serwisu (Auto: %c)\n", msg.data.brand);
        }
        sleep(1);
    }
}

int main() {
    pid_t pid;

    printf("=== Inicjalizacja IPC ===\n");

    msg_queue_id = msgget(QUEUE_KEY, IPC_CREAT | 0666);
    if(msg_queue_id == -1){
        perror("Błąd: nie powiodło się tworzenie kolejki.");
        exit(1);
    }
    printf("[System] Utworzono kolejkę komunikatów o id: %d\n", msg_queue_id);

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

    pid = fork();
    if(pid == 0) {
        run_client_generator();
    }else if(pid < 0) {
        perror("Błąd: nie powiodło się uruchomienie generatora klientów.");
        exit(1);
    }

    sleep(10);

    printf("\n=== Koniec symulacji ===\n");

    kill(0, SIGTERM); // na razie do testów, ubicie procesów w ten sposób

    if(msgctl(msg_queue_id, IPC_RMID, NULL) == -1) {
        perror("Błąd: nie powiodło się usuwanie kolejki.");
    }else{
        printf("[System] Usunięto kolejkę komunikatów.\n");
    }
    
    return 0;
}