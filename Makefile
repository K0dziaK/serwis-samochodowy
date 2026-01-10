CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99
OBJ = main.o common.o ipc_utils.o role_manager.o role_mechanic.o role_service.o role_generator.o
TARGET = service_app

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -pthread -o $(TARGET) $(OBJ)

main.o: main.c common.h ipc_utils.h roles.h
	$(CC) $(CFLAGS) -c main.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

ipc_utils.o: ipc_utils.c ipc_utils.h common.h
	$(CC) $(CFLAGS) -c ipc_utils.c

role_manager.o: role_manager.c roles.h common.h
	$(CC) $(CFLAGS) -pthread -c role_manager.c

role_mechanic.o: role_mechanic.c roles.h common.h ipc_utils.h
	$(CC) $(CFLAGS) -c role_mechanic.c

role_service.o: role_service.c roles.h common.h ipc_utils.h
	$(CC) $(CFLAGS) -c role_service.c

role_generator.o: role_generator.c roles.h common.h
	$(CC) $(CFLAGS) -c role_generator.c

clean:
	rm -f *.o $(TARGET)