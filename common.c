#include "common.h"

int msg_queue_id;
int sem_id;

const char ALLOWED_BRANDS[] = {'A','E','I','O','U','Y'};
const int NUM_ALLOWED_BRANDS = 6;

const Service SERVICE_LIST[] = {
    {"Wymiana oleju", 300, 3, 0},
    {"Wymiana klocków", 400, 4, 1},
    {"Naprawa silnika", 2000, 8, 1},
    {"Wymiana opon", 150, 2, 0},
    {"Awaria układu kierowniczego", 100, 2, 1},
    {"Naprawa hamulca ręcznego", 200, 3, 0},
    {"Naprawa opony", 50, 1, 0}
};
const int NUM_SERVICES = 7;