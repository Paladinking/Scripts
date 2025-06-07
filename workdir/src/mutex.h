#ifndef MUTEX_H_00
#define MUTEX_H_00
#include <stdbool.h>
#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


typedef struct Condition {
    CONDITION_VARIABLE cond;
    SRWLOCK mutex;
} Condition;

Condition* Condition_create();

void Condition_notify(Condition* cond);

void Condition_notify_all(Condition* cond);

void Condition_aquire(Condition* cond);

void Condition_release(Condition* cond);

bool Condition_wait(Condition* cond, uint32_t timeout);

void Condition_free(Condition* cond);

#endif
