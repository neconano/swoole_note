#include "swoole.h"

static int swCond_notify(swCond *cond);
static int swCond_broadcast(swCond *cond);
static int swCond_timewait(swCond *cond, long sec, long nsec);
static int swCond_wait(swCond *cond);
static void swCond_free(swCond *cond);

int swCond_create(swCond *cond)
{
    // 调用pthread_cond_init创建并初始化一个条件变量，并创建对应的互斥锁
    if (pthread_cond_init(&cond->cond, NULL) < 0)
    {
        swWarn("pthread_cond_init fail. Error: %s [%d]", strerror(errno), errno);
        return SW_ERR;
    }
    if (swMutex_create(&cond->lock, 0) < 0)
    {
        return SW_ERR;
    }

    cond->notify = swCond_notify;
    cond->broadcast = swCond_broadcast;
    cond->timewait = swCond_timewait;
    cond->wait = swCond_wait;
    cond->free = swCond_free;

    return SW_OK;
}

// 向另一个正处于阻塞等待状态的线程发信号，使其脱离阻塞状态
// 需要注意的是，如果有多个线程正在等待，
// 则根据优先级的高低决定由哪个线程收到信号；若优先级相同，则优先让等待时间最长的线程执行。
static int swCond_notify(swCond *cond)
{
    return pthread_cond_signal(&cond->cond);
}

// 向所有正处于阻塞等待状态的线程发出信号使其脱离阻塞状态
static int swCond_broadcast(swCond *cond)
{
    return pthread_cond_broadcast(&cond->cond);
}

// 计时等待其他线程发出信号，等待时间由参数long sec,long nsec指定
static int swCond_timewait(swCond *cond, long sec, long nsec)
{
    struct timespec timeo;

    timeo.tv_sec = sec;
    timeo.tv_nsec = nsec;

    return pthread_cond_timedwait(&cond->cond, &cond->lock.object.mutex._lock, &timeo);
}

// 等待其他线程发出信号
static int swCond_wait(swCond *cond)
{
    return pthread_cond_wait(&cond->cond, &cond->lock.object.mutex._lock);
}

// 销毁信号变量，并销毁互斥锁
static void swCond_free(swCond *cond)
{
    pthread_cond_destroy(&cond->cond);
    cond->lock.free(&cond->lock);
}
