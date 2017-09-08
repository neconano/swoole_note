#include "swoole.h"

static int swAtomicLock_lock(swLock *lock);
static int swAtomicLock_unlock(swLock *lock);
static int swAtomicLock_trylock(swLock *lock);

int swAtomicLock_create(swLock *lock, int spin)
{
    bzero(lock, sizeof(swLock));
    lock->type = SW_ATOMLOCK;
    lock->object.atomlock.spin = spin;
    lock->lock = swAtomicLock_lock;
    lock->unlock = swAtomicLock_unlock;
    lock->trylock = swAtomicLock_trylock;
    return SW_OK;
}

static int swAtomicLock_lock(swLock *lock)
{
    sw_spinlock(&lock->object.atomlock.lock_t);
    return SW_OK;
}

static int swAtomicLock_unlock(swLock *lock)
{
    return lock->object.atomlock.lock_t = 0;
}

static int swAtomicLock_trylock(swLock *lock)
{
    sw_atomic_t *atomic = &lock->object.atomlock.lock_t;
    return (*(atomic) == 0 && sw_atomic_cmp_set(atomic, 0, 1));
}
