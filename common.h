#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#define NUM_MECHANICS 8
#define MIN_SERVICE_DESKS 1
#define MAX_SERVICE_DESKS 3

const char ALLOWED_BRANDS[] = {'A','E','I','O','U','Y'};
const int NUM_ALLOWED_BRANDS = 6;

typedef enum {
    ROLE_MANAGER,
    ROLE_MECHANIC,
    ROLE_SERVICE,
    ROLE_CUSTOMER
} ProcessRole;

#endif