#include "swoole.h"

static int swPipeBase_read(swPipe *p, void *data, int length);
static int swPipeBase_write(swPipe *p, void *data, int length);
static int swPipeBase_getFd(swPipe *p, int isWriteFd);
static int swPipeBase_close(swPipe *p);

// swoole基于pipe函数用于创建一个管道,进行了一层封装用于简化操作
typedef struct _swPipeBase
{
    int pipes[2]; // 存放pipe的读端fd和写端fd
} swPipeBase;

int swPipeBase_create(swPipe *p, int blocking)
{
    int ret;
    swPipeBase *object = sw_malloc(sizeof(swPipeBase));
    if (object == NULL)
    {
        return -1;
    }
    p->blocking = blocking;
    ret = pipe(object->pipes);
    if (ret < 0)
    {
        swWarn("pipe create fail. Error: %s[%d]", strerror(errno), errno);
        return -1;
    }
    else
    {
        //Nonblock
        // 如果创建管道成功，若管道为非阻塞模式，则调用swSetNonBlock函数,设置两个fd为非阻塞
        swSetNonBlock(object->pipes[0]);
        swSetNonBlock(object->pipes[1]);
        p->timeout = -1;
        p->object = object;
        p->read = swPipeBase_read;
        p->write = swPipeBase_write;
        p->getFd = swPipeBase_getFd;
        p->close = swPipeBase_close;
    }
    return 0;
}

static int swPipeBase_read(swPipe *p, void *data, int length)
{
    swPipeBase *object = p->object;
    if (p->blocking == 1 && p->timeout > 0)
    {
        // 如果该管道为阻塞模式，且指定了timeout时间，则调用swSocket_wait函数执行等待，若超时，则返回SW_ERR
        if (swSocket_wait(object->pipes[0], p->timeout * 1000, SW_EVENT_READ) < 0)
            return SW_ERR;
    }
    // 此时，若管道为非阻塞模式，因已经指定了fd的options，
    // 所以read方法不会阻塞；若为阻塞模式，则read函数会一直阻塞到有数据可读
    return read(object->pipes[0], data, length);
}

static int swPipeBase_write(swPipe *p, void *data, int length)
{
    swPipeBase *this = p->object;
    return write(this->pipes[1], data, length);
}

static int swPipeBase_getFd(swPipe *p, int isWriteFd)
{
    swPipeBase *this = p->object;
    // 根据 isWriteFd 判定返回 写fd 或者 读fd
    return (isWriteFd == 0) ? this->pipes[0] : this->pipes[1];
}

static int swPipeBase_close(swPipe *p)
{
    int ret1, ret2;
    swPipeBase *this = p->object;
    ret1 = close(this->pipes[0]);
    ret2 = close(this->pipes[1]);
    sw_free(this);
    return 0 - ret1 - ret2;
}
