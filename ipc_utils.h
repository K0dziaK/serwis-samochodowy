#ifndef IPC_UTILS_H
#define IPC_UTILS_H

// Inicjalizacja kolejki na starcie systemu
void init_queue();

// Usuwanie kolejki przy wyłączaniu systemu
void clean_queue();

// Inicjalizacja semaforów na starcie systemu
void init_semaphores();

// Usuwanie semaforów przy wyłączaniu systemu
void clean_semaphores();

// Zajmowanie stanowiska dla danej marki (zwraca ID stanowiska lub -1)
int reserve_station(char brand);

// Zwalnianie stanowiska po naprawie
void release_station(int mechanic_id);

// Inicjalizacja i dostęp do pamięci dzielonej
SharedTime* attach_shared_time();
void init_shared_time();
void remove_shared_time();

#endif