#include "swoole.h"
#include <sys/sem.h>

static int swSem_lock(swLock *lock);
static int swSem_unlock(swLock *lock);
static int swSem_free(swLock *lock);

int swSem_create(swLock *lock, key_t key)
{
    int ret;
    assert(key != 0);
    lock->type = SW_SEM;
    if ((ret = semget(key, 1, IPC_CREAT | 0666)) < 0)
    {
        return SW_ERR;
    }

    if (semctl(ret, 0, SETVAL, 1) == -1)
    {
        swWarn("semctl(SETVAL) failed");
        return SW_ERR;
    }
    lock->object.sem.semid = ret;

    lock->lock = swSem_lock;
    lock->unlock = swSem_unlock;
    lock->free = swSem_free;

    return SW_OK;
}

static int swSem_unlock(swLock *lock)
{
    struct sembuf sem;
    sem.sem_flg = SEM_UNDO;
    sem.sem_num = 0;
    sem.sem_op = 1; // 设置sem_op为1（释放资源）
    return semop(lock->object.sem.semid, &sem, 1);
}

static int swSem_lock(swLock *lock)
{
    struct sembuf sem;
    sem.sem_flg = SEM_UNDO;
    sem.sem_num = 0;
    sem.sem_op = -1; // 设置sem_op为-1（请求资源）
    return semop(lock->object.sem.semid, &sem, 1);
}

static int swSem_free(swLock *lock)
{
    return semctl(lock->object.sem.semid, 0, IPC_RMID);
}
