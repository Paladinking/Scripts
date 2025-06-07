#include "mutex.h"
#include "mem.h"

Condition* Condition_create() {
    Condition* cond = Mem_alloc(sizeof(Condition));
    if (cond == NULL) {
        return NULL;
    }
    InitializeCriticalSection(&cond->mutex);
    InitializeConditionVariable(&cond->cond);

    return cond;
}

void Condition_notify(Condition* cond) {
    WakeConditionVariable(&cond->cond);
}


void Condition_notify_all(Condition* cond) {
    WakeAllConditionVariable(&cond->cond);
}

void Condition_aquire(Condition* cond) {
    EnterCriticalSection(&cond->mutex);
}

void Condition_release(Condition* cond) {
    LeaveCriticalSection(&cond->mutex);
}

bool Condition_wait(Condition* cond, uint32_t timeout) {
    return SleepConditionVariableCS(&cond->cond, &cond->mutex, timeout);
}

void Condition_free(Condition* cond) {
    DeleteCriticalSection(&cond->mutex);
    Mem_free(cond);
}
