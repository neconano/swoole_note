#include "swoole.h"

static int swMutex_lock(swLock *lock);
static int swMutex_unlock(swLock *lock);
static int swMutex_trylock(swLock *lock);
static int swMutex_free(swLock *lock);

int swMutex_create(swLock *lock, int use_in_process)
{
    int ret;
    bzero(lock, sizeof(swLock));
    lock->type = SW_MUTEX;
    // pthread_mutexattr_init方法用于初始化一个pthread_mutexattr_t结构体，
    // 并默认设置其pshared属性为PTHREAD_PROCESS_PRIVATE（表示可以在进程内使用该互斥锁）。
    pthread_mutexattr_init(&lock->object.mutex.attr);
    if (use_in_process == 1)
    {
        // 设置该锁的共享属性为PTHREAD_PROCESS_SHARED（进程间共享）
        // pthread_mutexattr_setpshared方法用来设置互斥锁变量的作用域。
        // PTHREAD_PROCESS_SHARED表示该互斥锁可被多个进程共享使用（前提是该互斥锁是在共享内存中创建）。
        // PTHREAD_PROCESS_PRIVATE表示该互斥锁仅能被那些由同一个进程创建的线程处理。
        pthread_mutexattr_setpshared(&lock->object.mutex.attr, PTHREAD_PROCESS_SHARED);
    }
    // pthread_mutex_init方法以动态方式创建互斥锁，并通过参数attr指定互斥锁属性。
    // 如果参数attr为空，则默认创建快速互斥锁。
    if ((ret = pthread_mutex_init(&lock->object.mutex._lock, &lock->object.mutex.attr)) < 0)
    {
        return SW_ERR;
    }
    lock->lock = swMutex_lock;
    lock->unlock = swMutex_unlock;
    lock->trylock = swMutex_trylock;
    lock->free = swMutex_free;
    return SW_OK;
}

static int swMutex_lock(swLock *lock)
{
    return pthread_mutex_lock(&lock->object.mutex._lock);
}

static int swMutex_unlock(swLock *lock)
{
    return pthread_mutex_unlock(&lock->object.mutex._lock);
}

static int swMutex_trylock(swLock *lock)
{
    return pthread_mutex_trylock(&lock->object.mutex._lock);
}

#ifdef HAVE_MUTEX_TIMEDLOCK
int swMutex_lockwait(swLock *lock, int timeout_msec)
{
    struct timespec timeo;
    timeo.tv_sec = timeout_msec / 1000;
    timeo.tv_nsec = (timeout_msec - timeo.tv_sec * 1000) * 1000 * 1000;
    return pthread_mutex_timedlock(&lock->object.mutex._lock, &timeo);
}
#else
int swMutex_lockwait(swLock *lock, int timeout_msec)
{
    int sub = 1;
    int sleep_ms = 1000;
    
    if (timeout_msec > 100)
    {
        sub = 10;
        sleep_ms = 10000;
    }
    
    while( timeout_msec > 0)
    {
        if (pthread_mutex_trylock(&lock->object.mutex._lock) == 0)
        {
            return 0;
        }
        else
        {
            usleep(sleep_ms);
            timeout_msec -= sub;
        }
    }
    return ETIMEDOUT;
}
#endif

static int swMutex_free(swLock *lock)
{
    return pthread_mutex_destroy(&lock->object.mutex._lock);
}
