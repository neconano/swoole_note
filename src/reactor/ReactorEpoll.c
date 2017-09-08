// epoll是Linux内核提供的一个多路复用I/O模型，它提供和poll函数一样的功能：监控多个文件描
// 述符是否处于I/O就绪状态（可读、可写）。这就是异步最核心的表现：程序不是主动等待一个描
// 述符可以操作，而是当描述符可操作时由系统提醒程序可以操作了，程序在被提醒前可以去做其他的事情
// 水平触发（Level-triggered）和边缘触发（edge-triggered）是epoll的两种模式，它们的区别在于：水平触发模式下，
// 当一个fd就绪之后，如果没有对该fd进行操作，则系统会继续发出就绪通知直到该fd被操作；
// 边缘触发模式下，当一个fd就绪后，系统仅会发出一次就绪通知。
#include "swoole.h"

#ifdef HAVE_EPOLL
#include <sys/epoll.h>

// EPOLLRDHUP代表的意义是对端断开连接，这个宏是用于弥补epoll在处理对端断开连接时可能会出现的一处Bug
#ifndef EPOLLRDHUP
#define EPOLLRDHUP   0x2000
#define NO_EPOLLRDHUP
#endif

// EPOLLONESHOT用于标记epoll对于每个socket仅监听一次事件，如果需要再次监听这个socket，需要再次将该socket加入epoll的监听队列中
#ifndef EPOLLONESHOT
#define EPOLLONESHOT (1u << 30)
#endif

typedef struct swReactorEpoll_s swReactorEpoll;

typedef struct _swFd
{
    uint32_t fd;
    uint32_t fdtype;
} swFd;

static int swReactorEpoll_add(swReactor *reactor, int fd, int fdtype);
static int swReactorEpoll_set(swReactor *reactor, int fd, int fdtype);
static int swReactorEpoll_del(swReactor *reactor, int fd);
static int swReactorEpoll_wait(swReactor *reactor, struct timeval *timeo);
static void swReactorEpoll_free(swReactor *reactor);

static sw_inline int swReactorEpoll_event_set(int fdtype)
{
    uint32_t flag = 0;
    if (swReactor_event_read(fdtype))
    {
        flag |= EPOLLIN;
    }
    if (swReactor_event_write(fdtype))
    {
        flag |= EPOLLOUT;
    }
    if (swReactor_event_error(fdtype))
    {
        //flag |= (EPOLLRDHUP);
        flag |= (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
    }
    return flag;
}

struct swReactorEpoll_s
{
    int epfd;
    struct epoll_event *events;
};

int swReactorEpoll_create(swReactor *reactor, int max_event_num)
{
    //create reactor object
    swReactorEpoll *reactor_object = sw_malloc(sizeof(swReactorEpoll));
    if (reactor_object == NULL)
    {
        swWarn("malloc[0] failed.");
        return SW_ERR;
    }
    bzero(reactor_object, sizeof(swReactorEpoll));
    reactor->object = reactor_object;
    reactor->max_event_num = max_event_num;

    reactor_object->events = sw_calloc(max_event_num, sizeof(struct epoll_event));

    if (reactor_object->events == NULL)
    {
        swWarn("malloc[1] failed.");
        return SW_ERR;
    }
    //epoll create
    reactor_object->epfd = epoll_create(512);
    if (reactor_object->epfd < 0)
    {
        swWarn("epoll_create failed. Error: %s[%d]", strerror(errno), errno);
        return SW_ERR;
    }
    //binding method
    reactor->add = swReactorEpoll_add;
    reactor->set = swReactorEpoll_set;
    reactor->del = swReactorEpoll_del;
    reactor->wait = swReactorEpoll_wait;
    reactor->free = swReactorEpoll_free;

    return SW_OK;
}

static void swReactorEpoll_free(swReactor *reactor)
{
    swReactorEpoll *object = reactor->object;
    // 关闭epoll实例的描述符，并释放申请的内存空间
    close(object->epfd);
    sw_free(object->events);
    sw_free(object);
}

static int swReactorEpoll_add(swReactor *reactor, int fd, int fdtype)
{
    if (swReactor_add(reactor, fd, fdtype) < 0)
    {
        return SW_ERR;
    }

    swReactorEpoll *object = reactor->object;
    struct epoll_event e;
    swFd fd_;
    int ret;
    bzero(&e, sizeof(struct epoll_event));

    fd_.fd = fd;
    fd_.fdtype = swReactor_fdtype(fdtype);
    e.events = swReactorEpoll_event_set(fdtype);

    if (e.events & EPOLLOUT)
    {
        assert(fd > 2);
    }

    memcpy(&(e.data.u64), &fd_, sizeof(fd_));
    ret = epoll_ctl(object->epfd, EPOLL_CTL_ADD, fd, &e);
    if (ret < 0)
    {
        swSysError("add events[fd=%d#%d, type=%d, events=%d] failed.", fd, reactor->id, fd_.fdtype, e.events);
        return SW_ERR;
    }
    swTraceLog(SW_TRACE_EVENT, "add event[reactor_id=%d|fd=%d]", reactor->id, fd);
    reactor->event_num++;
    return SW_OK;
}

static int swReactorEpoll_del(swReactor *reactor, int fd)
{
    swReactorEpoll *object = reactor->object;
    int ret;

    if (fd <= 0)
    {
        return SW_ERR;
    }
    ret = epoll_ctl(object->epfd, EPOLL_CTL_DEL, fd, NULL);
    if (ret < 0)
    {
        swSysError("epoll remove fd[%d#%d] failed.", fd, reactor->id);
        return SW_ERR;
    }

    if (swReactor_del(reactor, fd) < 0)
    {
        return SW_ERR;
    }

    reactor->event_num = reactor->event_num <= 0 ? 0 : reactor->event_num - 1;
    swTraceLog(SW_TRACE_EVENT, "remove event[reactor_id=%d|fd=%d]", reactor->id, fd);
    return SW_OK;
}

static int swReactorEpoll_set(swReactor *reactor, int fd, int fdtype)
{
    swReactorEpoll *object = reactor->object;
    swFd fd_;
    struct epoll_event e;
    int ret;

    bzero(&e, sizeof(struct epoll_event));
    e.events = swReactorEpoll_event_set(fdtype);

    if (e.events & EPOLLOUT)
    {
        assert(fd > 2);
    }

    fd_.fd = fd;
    fd_.fdtype = swReactor_fdtype(fdtype);
    memcpy(&(e.data.u64), &fd_, sizeof(fd_));

    ret = epoll_ctl(object->epfd, EPOLL_CTL_MOD, fd, &e);
    if (ret < 0)
    {
        swSysError("reactor#%d->set(fd=%d|type=%d|events=%d) failed.", reactor->id, fd, fd_.fdtype, e.events);
        return SW_ERR;
    }
    //execute parent method
    swReactor_set(reactor, fd, fdtype);
    return SW_OK;
}


static int swReactorEpoll_wait(swReactor *reactor, struct timeval *timeo)
{
        // event是相应事件数据的封装，是回调函数handle的第二个参数
        swEvent event;
        swReactorEpoll *object = reactor->object;
        swReactor_handle handle;
        // n为每一次epoll_wait响应后返回的当前处于就绪状态的fd的数量
        int i, n, ret, msec;

        int reactor_id = reactor->id;
        int epoll_fd = object->epfd;
        int max_event_num = reactor->max_event_num;
        // events用于存放epoll函数发现的处于就绪状态的事件
        struct epoll_event *events = object->events;
        if (reactor->timeout_msec == 0)
        {
            if (timeo == NULL)
                reactor->timeout_msec = -1;
            else
                reactor->timeout_msec = timeo->tv_sec * 1000 + timeo->tv_usec / 1000;
        }
        while (reactor->running > 0)
        {
            msec = reactor->timeout_msec;
            // 调用epoll_wait函数获取已经处于就绪状态的fd的集合，该集合存放在events结构体数组中，其数目为返回值n
            // 如果没有任何描述符处于就绪状态，该函数会阻塞直到有描述符就绪
            // 如果在usec毫秒后仍没有描述符就绪，则返回0
            n = epoll_wait(epoll_fd, events, max_event_num, msec);
            if (n < 0)
            {
                if (swReactor_error(reactor) < 0)
                {
                    swWarn("[Reactor#%d] epoll_wait failed. Error: %s[%d]", reactor_id, strerror(errno), errno);
                    return SW_ERR;
                }
                else
                    continue;
            }
            else if (n == 0)
            {
                if (reactor->onTimeout != NULL)
                    reactor->onTimeout(reactor);
                continue;
            }
            for (i = 0; i < n; i++)
            {
                event.fd = events[i].data.u64;
                event.from_id = reactor_id;
                // 低位32位存放的是uint32_t类型的fd
                // 高位32位存放的是uint32_t类型的fdtype，该fdtype的取值为枚举类型swFd_type
                event.type = events[i].data.u64 >> 32;
                event.socket = swReactor_get(reactor, event.fd);

                //read
                // EPOLLIN 读事件，根据swEvent中的type类型（fdtype）获取对应的读操作的回调函数，通过该回调将swEvent类型发出
                if (events[i].events & EPOLLIN)
                {
                    handle = swReactor_getHandle(reactor, SW_EVENT_READ, event.type);
                    ret = handle(reactor, &event);
                    if (ret < 0)
                        swSysError("EPOLLIN handle failed. fd=%d.", event.fd);
                }
                //write
                // EPOLLOUT 写事件，根据swEvent中的type类型（fdtype）获取对应的写操作的回调函数，通过该回调将swEvent类型发出
                if ((events[i].events & EPOLLOUT) && event.socket->fd && !event.socket->removed)
                {
                    handle = swReactor_getHandle(reactor, SW_EVENT_WRITE, event.type);
                    ret = handle(reactor, &event);
                    if (ret < 0)
                        swSysError("EPOLLOUT handle failed. fd=%d.", event.fd);
                }
                //error
                // 异常事件，根据swEvent中的type类型（fdtype）获取对应的异常操作的回调函数，通过该回调将swEvent类型发出
                #ifndef NO_EPOLLRDHUP
                if ((events[i].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) && event.socket->fd && !event.socket->removed)
                #else
                if ((events[i].events & (EPOLLERR | EPOLLHUP)) && !event.socket->removed)
                #endif
                {
                    handle = swReactor_getHandle(reactor, SW_EVENT_ERROR, event.type);
                    ret = handle(reactor, &event);
                    if (ret < 0)
                        swSysError("EPOLLERR handle failed. fd=%d.", event.fd);
                }
            }
            if (reactor->onFinish != NULL)
                reactor->onFinish(reactor);
        }
        return 0;
}

#endif
