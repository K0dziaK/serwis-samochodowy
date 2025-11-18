CC = gcc
CFLAGS = -Wall -std=gnu99
TARGET = serwis

all: $(TARGET)

$(TARGET): main.c common.h
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)