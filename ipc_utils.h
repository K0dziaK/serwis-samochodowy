#ifndef IPC_UTILS_H
#define IPC_UTILS_H

// Inicjalizacja semaforów na starcie systemu
void init_semaphores();

// Zajmowanie stanowiska dla danej marki (zwraca ID stanowiska lub -1)
int reserve_station(char brand);

// Zwalnianie stanowiska po naprawie
void release_station(int mechanic_id);

#endif