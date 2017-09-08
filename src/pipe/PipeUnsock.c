#include "swoole.h"
#include "buffer.h"
#include <sys/ipc.h>
#include <sys/msg.h>

static int swPipeUnsock_read(swPipe *p, void *data, int length);
static int swPipeUnsock_write(swPipe *p, void *data, int length);
static int swPipeUnsock_getFd(swPipe *p, int isWriteFd);
static int swPipeUnsock_close(swPipe *p);

// PipeUnsock使用了socketpair函数和AF_UNIX（Unix Socket）来创建一个全双工的“管道”
typedef struct _swPipeUnsock
{
    int socks[2];
} swPipeUnsock;

static int swPipeUnsock_getFd(swPipe *p, int isWriteFd)
{
    swPipeUnsock *this = p->object;
    return isWriteFd == 1 ? this->socks[1] : this->socks[0];
}

static int swPipeUnsock_close(swPipe *p)
{
    int ret1, ret2;
    swPipeUnsock *object = p->object;

    ret1 = close(object->socks[0]);
    ret2 = close(object->socks[1]);

    sw_free(object);

    return 0 - ret1 - ret2;
}

int swPipeUnsock_create(swPipe *p, int blocking, int protocol)
{
    int ret;
    swPipeUnsock *object = sw_malloc(sizeof(swPipeUnsock));
    if (object == NULL)
    {
        swWarn("malloc() failed.");
        return SW_ERR;
    }
    // 设置管道阻塞类型
    p->blocking = blocking;
    // 调用socketpair获取两个套接字，并指定套接字类型为AF_UNIX
    ret = socketpair(AF_UNIX, protocol, 0, object->socks);
    if (ret < 0)
    {
        swWarn("socketpair() failed. Error: %s [%d]", strerror(errno), errno);
        return SW_ERR;
    }
    else
    {
        //Nonblock
        // 如果管道为非阻塞类型，则调用swSetNonBlock函数设置套接字属性
        if (blocking == 0)
        {
            swSetNonBlock(object->socks[0]);
            swSetNonBlock(object->socks[1]);
        }

        // 然后指定socket buffer 的大小
        int sbsize = SwooleG.socket_buffer_size;
        swSocket_set_buffer_size(object->socks[0], sbsize);
        swSocket_set_buffer_size(object->socks[1], sbsize);

        p->object = object;
        p->read = swPipeUnsock_read;
        p->write = swPipeUnsock_write;
        p->getFd = swPipeUnsock_getFd;
        p->close = swPipeUnsock_close;
    }
    return 0;
}

static int swPipeUnsock_read(swPipe *p, void *data, int length)
{
    return read(((swPipeUnsock *) p->object)->socks[0], data, length);
}

static int swPipeUnsock_write(swPipe *p, void *data, int length)
{
    return write(((swPipeUnsock *) p->object)->socks[1], data, length);
}
