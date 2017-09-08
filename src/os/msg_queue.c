#include "swoole.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

void swMsgQueue_free(swMsgQueue *q)
{
    if (q->delete)
    {
        // 调用msgctl释放一个消息队列
        // IPC_RMID参数代表立即释放全部资源
        msgctl(q->msg_id, IPC_RMID, 0);
    }
    sw_free(q);
}

void swMsgQueue_set_blocking(swMsgQueue *q, uint8_t blocking)
{
    q->ipc_wait = blocking ? 0 : IPC_NOWAIT;
}

int swMsgQueue_create(swMsgQueue *q, int blocking, key_t msg_key, long type)
{
    int msg_id;
    if (blocking == 0)
        q->ipc_wait = IPC_NOWAIT;
    else
        q->ipc_wait = 0;
    q->blocking = blocking;
    // 调用msgget函数创建消息队列
    // IPC_CREAT表明，若不存在以key为索引的消息队列，则创建新的队列
    // 否则，返回已创建的消息队列的id
    // O_EXCL表示若打开一个已存在的消息队列，则返回-1
    // 0666为该消息队列的读写权限
    msg_id = msgget(msg_key, IPC_CREAT | O_EXCL | 0666);
    if (msg_id < 0)
    {
        swWarn("msgget() failed. Error: %s[%d]", strerror(errno), errno);
        return SW_ERR;
    }
    else
    {
        q->msg_id = msg_id;
        q->type = type;
    }
    return 0;
}

int swMsgQueue_pop(swMsgQueue *q, swQueue_data *data, int length)
{
    int flag = q->ipc_wait;
    long type = data->mtype;
    // 调用msgrcv函数接收指定类型的消息
    return msgrcv(q->msg_id, data, length, type, flag);
}

int swMsgQueue_push(swMsgQueue *q, swQueue_data *in, int length)
{
    int ret;
    while (1)
    {
        // 循环调用msgsnd直到消息发送成功或无法再次发送
        ret = msgsnd(q->msg_id, in, length, q->ipc_wait);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
            {
                swYield();
                continue;
            }
            else
                return -1;
        }
        else
            return ret;
    }
    return 0;
}
