CC = gcc
CFLAGS = -Wall -Wextra -pthread

# Programy wykonywalne (wymagane przez fork+exec)
PROGRAMS = simulation mechanic service cashier client manager generator

# Obiekty dla głównego programu
MAIN_OBJ = main.o common.o

# Obiekty dla poszczególnych ról
MECHANIC_OBJ = mechanic_main.o common.o role_mechanic.o
SERVICE_OBJ = service_main.o common.o role_service.o
CASHIER_OBJ = cashier_main.o common.o role_cashier.o
CLIENT_OBJ = client_main.o common.o role_client.o
MANAGER_OBJ = manager_main.o common.o role_manager.o role_service.o
GENERATOR_OBJ = generator_main.o common.o

all: $(PROGRAMS)

# Główny program symulacji (tylko uruchamia exec)
simulation: $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o simulation $(MAIN_OBJ)

# Osobne programy wykonywalne dla każdej roli
mechanic: $(MECHANIC_OBJ)
	$(CC) $(CFLAGS) -o mechanic $(MECHANIC_OBJ)

service: $(SERVICE_OBJ)
	$(CC) $(CFLAGS) -o service $(SERVICE_OBJ)

cashier: $(CASHIER_OBJ)
	$(CC) $(CFLAGS) -o cashier $(CASHIER_OBJ)

client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJ)

manager: $(MANAGER_OBJ)
	$(CC) $(CFLAGS) -o manager $(MANAGER_OBJ)

generator: $(GENERATOR_OBJ)
	$(CC) $(CFLAGS) -o generator $(GENERATOR_OBJ)

# Kompilacja obiektów
main.o: main.c common.h
	$(CC) $(CFLAGS) -c main.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

mechanic_main.o: mechanic_main.c common.h
	$(CC) $(CFLAGS) -c mechanic_main.c

service_main.o: service_main.c common.h
	$(CC) $(CFLAGS) -c service_main.c

cashier_main.o: cashier_main.c common.h
	$(CC) $(CFLAGS) -c cashier_main.c

client_main.o: client_main.c common.h
	$(CC) $(CFLAGS) -c client_main.c

manager_main.o: manager_main.c common.h
	$(CC) $(CFLAGS) -c manager_main.c

generator_main.o: generator_main.c common.h
	$(CC) $(CFLAGS) -c generator_main.c

role_mechanic.o: role_mechanic.c common.h
	$(CC) $(CFLAGS) -c role_mechanic.c

role_service.o: role_service.c common.h
	$(CC) $(CFLAGS) -c role_service.c

role_client.o: role_client.c common.h
	$(CC) $(CFLAGS) -c role_client.c

role_cashier.o: role_cashier.c common.h
	$(CC) $(CFLAGS) -c role_cashier.c

role_manager.o: role_manager.c common.h
	$(CC) $(CFLAGS) -c role_manager.c

clean:
	rm -f *.o $(PROGRAMS) raport.txt