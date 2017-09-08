#include "swoole.h"

#ifdef HAVE_EVENTFD
#include <sys/eventfd.h>

static int swPipeEventfd_read(swPipe *p, void *data, int length);
static int swPipeEventfd_write(swPipe *p, void *data, int length);
static int swPipeEventfd_getFd(swPipe *p, int isWriteFd);
static int swPipeEventfd_close(swPipe *p);

typedef struct _swPipeEventfd
{
    int event_fd;
} swPipeEventfd;

// 第三个参数semaphore标记该管道是否仅用于提供通知
int swPipeEventfd_create(swPipe *p, int blocking, int semaphore, int timeout)
{
    int efd;
    int flag = 0;
    // 创建一个swPipeEventfd对象，默认设置管道为非阻塞的
    swPipeEventfd *object = sw_malloc(sizeof(swPipeEventfd));
    if (object == NULL)
        return -1;
    flag = EFD_NONBLOCK;

    if (blocking == 1)
    {
        if (timeout > 0)
        {
            flag = 0;
            p->timeout = -1;
        }
        else
            p->timeout = timeout;
    }

    #ifdef EFD_SEMAPHORE
        if (semaphore == 1)
            flag |= EFD_SEMAPHORE;
    #endif

    p->blocking = blocking;
    // eventfd函数创建一个对象用于事件的等待/提醒，可以从内核态发送通知到用户态
    // 一个eventfd创建的对象包括一个无符号的64bit整型计数器，该计数器被内核持有
    // eventfd函数的第二个参数用于指定flags，其中EFD_NONBLOCK指定eventfd是否非阻塞，EFD_SEMAPHORE用于指定eventfd的读写效果
    efd = eventfd(0, flag);
    if (efd < 0)
    {
        swWarn("eventfd create failed. Error: %s[%d]", strerror(errno), errno);
        return -1;
    }
    else
    {
        p->object = object;
        p->read = swPipeEventfd_read;
        p->write = swPipeEventfd_write;
        p->getFd = swPipeEventfd_getFd;
        p->close = swPipeEventfd_close;
        object->event_fd = efd;
    }
    return 0;
}

static int swPipeEventfd_read(swPipe *p, void *data, int length)
{
    int ret = -1;
    swPipeEventfd *object = p->object;

    //eventfd not support socket timeout
    if (p->blocking == 1 && p->timeout > 0)
    {
        if (swSocket_wait(object->event_fd, p->timeout * 1000, SW_EVENT_READ) < 0)
            return SW_ERR;
    }

    while (1)
    {
        // 循环调用read函数直到读取到数据
        ret = read(object->event_fd, data, sizeof(uint64_t));
        if (ret < 0 && errno == EINTR)
            continue;
        break;
    }
    return ret;
}

static int swPipeEventfd_write(swPipe *p, void *data, int length)
{
    int ret;
    swPipeEventfd *this = p->object;
    while (1)
    {
        // 循环调用write函数直到写入数据成功
        ret = write(this->event_fd, data, sizeof(uint64_t));
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
        }
        break;
    }
    return ret;
}

// 返回event_fd
static int swPipeEventfd_getFd(swPipe *p, int isWriteFd)
{
    return ((swPipeEventfd *) (p->object))->event_fd;
}

// 调用close关闭event_fd并释放内存
static int swPipeEventfd_close(swPipe *p)
{
    int ret;
    ret = close(((swPipeEventfd *) (p->object))->event_fd);
    sw_free(p->object);
    return ret;
}

#endif
