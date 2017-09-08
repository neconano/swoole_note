#include "swoole.h"

#ifdef HAVE_RWLOCK

static int swRWLock_lock_rd(swLock *lock);
static int swRWLock_lock_rw(swLock *lock);
static int swRWLock_unlock(swLock *lock);
static int swRWLock_trylock_rw(swLock *lock);
static int swRWLock_trylock_rd(swLock *lock);
static int swRWLock_free(swLock *lock);

int swRWLock_create(swLock *lock, int use_in_process)
{
    int ret;
    bzero(lock, sizeof(swLock));
    // 设置锁类型为SW_RWLOCK，如果该锁用于进程之间
    lock->type = SW_RWLOCK;
    if (use_in_process == 1)
    {
        // 设置该锁的共享属性为PTHREAD_PROCESS_SHARED（进程间共享）
        // pthread_rwlockattr_setpshared用于设置一个pthread_rwlockattr_t的属性，
        // 和mutex一样有PTHREAD_PROCESS_SHARED和PTHREAD_PROCESS_PRIVATE两个值。
        pthread_rwlockattr_setpshared(&lock->object.rwlock.attr, PTHREAD_PROCESS_SHARED);
    }
    // pthread_rwlock_init用于初始化一个pthread_rwlock_t结构体，创建一个读写锁，并通过attr设置其属性。 
    // （实际上rwlock也有pthread_rwlockattr_init方法，不知道为什么这里没有调用。
    if ((ret = pthread_rwlock_init(&lock->object.rwlock._lock, &lock->object.rwlock.attr)) < 0)
    {
        return SW_ERR;
    }
    lock->lock_rd = swRWLock_lock_rd;
    lock->lock = swRWLock_lock_rw;
    lock->unlock = swRWLock_unlock;
    lock->trylock = swRWLock_trylock_rw;
    lock->trylock_rd = swRWLock_trylock_rd;
    lock->free = swRWLock_free;
    return SW_OK;
}

static int swRWLock_lock_rd(swLock *lock)
{
    return pthread_rwlock_rdlock(&lock->object.rwlock._lock);
}

static int swRWLock_lock_rw(swLock *lock)
{
    return pthread_rwlock_wrlock(&lock->object.rwlock._lock);
}

static int swRWLock_unlock(swLock *lock)
{
    return pthread_rwlock_unlock(&lock->object.rwlock._lock);
}

static int swRWLock_trylock_rd(swLock *lock)
{
    return pthread_rwlock_tryrdlock(&lock->object.rwlock._lock);
}

static int swRWLock_trylock_rw(swLock *lock)
{
    return pthread_rwlock_trywrlock(&lock->object.rwlock._lock);
}

static int swRWLock_free(swLock *lock)
{
    return pthread_rwlock_destroy(&lock->object.rwlock._lock);
}

#endif
