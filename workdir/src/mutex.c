#include "mutex.h"
#include "mem.h"

Condition* Condition_create() {
    Condition* cond = Mem_alloc(sizeof(Condition));
    if (cond == NULL) {
        return NULL;
    }
    InitializeSRWLock(&cond->mutex);
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
    AcquireSRWLockExclusive(&cond->mutex);
}

void Condition_release(Condition* cond) {
    ReleaseSRWLockExclusive(&cond->mutex);
}

bool Condition_wait(Condition* cond, uint32_t timeout) {
    return SleepConditionVariableSRW(&cond->cond, &cond->mutex, timeout, 0);
}

void Condition_free(Condition* cond) {
    Mem_free(cond);
}
