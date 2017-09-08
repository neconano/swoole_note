#include "swoole.h"

// 其中rd代表读锁，rw代表写锁
static int swFileLock_lock_rd(swLock *lock);
static int swFileLock_lock_rw(swLock *lock);
static int swFileLock_unlock(swLock *lock);
static int swFileLock_trylock_rw(swLock *lock);
static int swFileLock_trylock_rd(swLock *lock);
static int swFileLock_free(swLock *lock);

int swFileLock_create(swLock *lock, int fd)
{
    bzero(lock, sizeof(swLock));
    lock->type = SW_FILELOCK;
    lock->object.filelock.fd = fd;
    lock->lock_rd = swFileLock_lock_rd;
    lock->lock = swFileLock_lock_rw;
    lock->trylock_rd = swFileLock_trylock_rd;
    lock->trylock = swFileLock_trylock_rw;
    lock->unlock = swFileLock_unlock;
    lock->free = swFileLock_free;
    return 0;
}

// 申请文件读锁RDLOCK
static int swFileLock_lock_rd(swLock *lock)
{
    lock->object.filelock.lock_t.l_type = F_RDLCK;
    // 测试锁（http://www.cnblogs.com/papam/archive/2009/09/02/1559154.html）
    return fcntl(lock->object.filelock.fd, F_SETLKW, &lock->object.filelock);
}

// 申请文件写锁
static int swFileLock_lock_rw(swLock *lock)
{
    lock->object.filelock.lock_t.l_type = F_WRLCK;
    return fcntl(lock->object.filelock.fd, F_SETLKW, &lock->object.filelock);
}

// 释放文件锁
static int swFileLock_unlock(swLock *lock)
{
    lock->object.filelock.lock_t.l_type = F_UNLCK;
    return fcntl(lock->object.filelock.fd, F_SETLKW, &lock->object.filelock);
}

// 尝试是否能申请写锁
static int swFileLock_trylock_rw(swLock *lock)
{
    lock->object.filelock.lock_t.l_type = F_WRLCK;
    return fcntl(lock->object.filelock.fd, F_SETLK, &lock->object.filelock);
}

// 尝试是否能申请读锁
static int swFileLock_trylock_rd(swLock *lock)
{
    lock->object.filelock.lock_t.l_type = F_RDLCK;
    return fcntl(lock->object.filelock.fd, F_SETLK, &lock->object.filelock);
}

// 调用close函数释放锁
static int swFileLock_free(swLock *lock)
{
    return close(lock->object.filelock.fd);
}
