#include "common.h"

int msg_queue_id;
int sem_id;

const char ALLOWED_BRANDS[] = {'A','E','I','O','U','Y'};
const int NUM_ALLOWED_BRANDS = 6;

const Service SERVICE_LIST[] = {
    {"Wymiana oleju", 300, 3},
    {"Wymiana klocków", 400, 4},
    {"Naprawa silnika", 2000, 8},
    {"Wymiana opon", 150, 2},
    {"Diagnostyka", 100, 2}
};
const int NUM_SERVICES = 5;