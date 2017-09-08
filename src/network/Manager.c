#include "swoole.h"
#include "Server.h"

#include <sys/wait.h>

typedef struct
{
    uint8_t reloading;
    uint8_t reload_event_worker;
    uint8_t reload_task_worker;

} swManagerProcess;

static int swManager_loop(swFactory *factory);
static void swManager_signal_handle(int sig);
static pid_t swManager_spawn_worker(swFactory *factory, int worker_id);
static void swManager_check_exit_status(swServer *serv, int worker_id, pid_t pid, int status);

static swManagerProcess ManagerProcess;

// create worker child proccess
// 启动一个manager进程，该manager进程用于创建和管理每个worker进程
int swManager_start(swFactory *factory)
{
        swFactoryProcess *object = factory->object;
        int i, ret;
        pid_t pid;
        swServer *serv = factory->ptr;
        // 创建通讯管道（使用unix sock管道，以此保证管道是双向可用的）
        object->pipes = sw_calloc(serv->worker_num, sizeof(swPipe));
        if (object->pipes == NULL)
        {
            swError("malloc[worker_pipes] failed. Error: %s [%d]", strerror(errno), errno);
            return SW_ERR;
        }
        //worker进程的pipes
        for (i = 0; i < serv->worker_num; i++)
        {
            if (swPipeUnsock_create(&object->pipes[i], 1, SOCK_DGRAM) < 0)
                return SW_ERR;
            serv->workers[i].pipe_master = object->pipes[i].getFd(&object->pipes[i], SW_PIPE_MASTER);
            serv->workers[i].pipe_worker = object->pipes[i].getFd(&object->pipes[i], SW_PIPE_WORKER);
            serv->workers[i].pipe_object = &object->pipes[i];
            swServer_pipe_set(serv, serv->workers[i].pipe_object);
        }
        if (SwooleG.task_worker_num > 0)
        {
                key_t key = 0;
                int create_pipe = 1;
                if (SwooleG.task_ipc_mode > SW_TASK_IPC_UNIXSOCK)
                {
                    key = serv->message_queue_key + 2;
                    create_pipe = 0;
                }
                // 创建一个PorcessPool进程池用于管理多个task进程
                if (swProcessPool_create(&SwooleGS->task_workers, SwooleG.task_worker_num, SwooleG.task_max_request, key, create_pipe) < 0)
                {
                    swWarn("[Master] create task_workers failed.");
                    return SW_ERR;
                }
                swProcessPool *pool = &SwooleGS->task_workers;
                swTaskWorker_init(pool);
                swWorker *worker;
                // 循环创建指定个数的task进程并设置swServer指针和回调函数
                for (i = 0; i < SwooleG.task_worker_num; i++)
                {
                    worker = &pool->workers[i];
                    if (swWorker_create(worker) < 0)
                        return SW_ERR;
                    if (SwooleG.task_ipc_mode == SW_IPC_UNSOCK)
                        swServer_pipe_set(SwooleG.serv, worker->pipe_object);
                }
        }
        //User Worker Process
        if (serv->user_worker_num > 0)
        {
            serv->user_workers = sw_calloc(serv->user_worker_num, sizeof(swWorker *));
            swUserWorker_node *user_worker;
            i = 0;
            LL_FOREACH(serv->user_worker_list, user_worker)
            {
                if (swWorker_create(user_worker->worker) < 0)
                    return SW_ERR;
                serv->user_workers[i++] = user_worker->worker;
            }
        }
        pid = fork();
        switch (pid)
        {
            //创建manager进程
            case 0:
                //wait master process
                SW_START_SLEEP;
                if (SwooleGS->start == 0)
                    return SW_OK;
                /**
                * create worker process 
                */
                for (i = 0; i < serv->worker_num; i++)
                {
                    //close(worker_pipes[i].pipes[0]);
                    // 创建子进程
                    pid = swManager_spawn_worker(factory, i);
                    if (pid < 0)
                    {
                        swError("fork() failed.");
                        return SW_ERR;
                    }
                    else
                        serv->workers[i].pid = pid;
                }
                /**
                * create task worker process
                */
                if (SwooleG.task_worker_num > 0){
                    // 启动task进程池
                    swProcessPool_start(&SwooleGS->task_workers);
                }
                /**
                * create user worker process
                */
                if (serv->user_worker_list)
                {
                    swUserWorker_node *user_worker;
                    LL_FOREACH(serv->user_worker_list, user_worker)
                    {
                        /**
                        * store the pipe object
                        */
                        if (user_worker->worker->pipe_object)
                            swServer_pipe_set(serv, user_worker->worker->pipe_object);
                        swManager_spawn_user_worker(serv, user_worker->worker);
                    }
                }
                //标识为管理进程
                SwooleG.process_type = SW_PROCESS_MANAGER;
                SwooleG.pid = getpid();

                ret = swManager_loop(factory);
                exit(ret);
                break;
                //master process
            default:
                SwooleGS->manager_pid = pid;
                break;
            case -1:
                swError("fork() failed.");
                return SW_ERR;
        }
        return SW_OK;
}

static void swManager_check_exit_status(swServer *serv, int worker_id, pid_t pid, int status)
{
    if (!WIFEXITED(status))
    {
        swWarn("worker#%d abnormal exit, status=%d, signal=%d", worker_id, WEXITSTATUS(status), WTERMSIG(status));

        if (serv->onWorkerError != NULL)
        {
            serv->onWorkerError(serv, worker_id, pid, WEXITSTATUS(status), WTERMSIG(status));
        }
    }
}

static int swManager_loop(swFactory *factory)
{
        int pid, new_pid;
        int i;
        int reload_worker_i = 0;
        int reload_worker_num;
        int ret;
        int status;

        SwooleG.use_signalfd = 0;
        SwooleG.use_timerfd = 0;
        memset(&ManagerProcess, 0, sizeof(ManagerProcess));
        swServer *serv = factory->ptr;
        swWorker *reload_workers;

        if (serv->onManagerStart)
            serv->onManagerStart(serv);

        reload_worker_num = serv->worker_num + SwooleG.task_worker_num;
        reload_workers = sw_calloc(reload_worker_num, sizeof(swWorker));
        if (reload_workers == NULL)
        {
            swError("malloc[reload_workers] failed");
            return SW_ERR;
        }

        //for reload
        swSignal_add(SIGHUP, NULL);
        swSignal_add(SIGTERM, swManager_signal_handle);
        swSignal_add(SIGUSR1, swManager_signal_handle);
        swSignal_add(SIGUSR2, swManager_signal_handle);
        //swSignal_add(SIGINT, swManager_signal_handle);
        SwooleG.main_reactor = NULL;

        while (SwooleG.running > 0)
        {
            // 调用wait等待worker_exit事件的发生
            pid = wait(&status);
            // wait函数出错
            if (pid < 0)
            {
                if (ManagerProcess.reloading == 0)
                    swTrace("wait() failed. Error: %s [%d]", strerror(errno), errno);
                else if (ManagerProcess.reload_event_worker == 1)
                {
                    swNotice("Server is reloading now.");
                    memcpy(reload_workers, serv->workers, sizeof(swWorker) * serv->worker_num);
                    reload_worker_num = serv->worker_num;
                    if (SwooleG.task_worker_num > 0)
                    {
                        memcpy(reload_workers + serv->worker_num, SwooleGS->task_workers.workers,
                                sizeof(swWorker) * SwooleG.task_worker_num);
                        reload_worker_num += SwooleG.task_worker_num;
                    }
                    reload_worker_i = 0;
                    ManagerProcess.reload_event_worker = 0;
                    goto kill_worker;
                }
                else if (ManagerProcess.reload_task_worker == 1)
                {
                    swNotice("Server is reloading now.");
                    if (SwooleG.task_worker_num == 0)
                    {
                        swWarn("cannot reload workers, because server no have task workers.");
                        continue;
                    }
                    memcpy(reload_workers, SwooleGS->task_workers.workers, sizeof(swWorker) * SwooleG.task_worker_num);
                    reload_worker_num = SwooleG.task_worker_num;
                    reload_worker_i = 0;
                    ManagerProcess.reload_task_worker = 0;
                    goto kill_worker;
                }
            }
            if (SwooleG.running == 1)
            {
                for (i = 0; i < serv->worker_num; i++)
                {
                    //compare PID
                    if (pid != serv->workers[i].pid)
                        continue;
                    else
                    {
                        swManager_check_exit_status(serv, i, pid, status);
                        pid = 0;
                        while (1)
                        {
                            new_pid = swManager_spawn_worker(factory, i);
                            if (new_pid < 0)
                            {
                                usleep(100000);
                                continue;
                            }
                            else
                            {
                                serv->workers[i].pid = new_pid;
                                break;
                            }
                        }
                    }
                }

                if (pid > 0)
                {
                    swWorker *exit_worker;
                    //task worker
                    if (SwooleGS->task_workers.map)
                    {
                        // 通过swHashMap_find_int函数从SwooleG全局变量的task_workers里的map中找到对应的task_worker结构体
                        exit_worker = swHashMap_find_int(SwooleGS->task_workers.map, pid);
                        if (exit_worker != NULL)
                        {
                            swManager_check_exit_status(serv, exit_worker->id, pid, status);
                            if (exit_worker->deleted == 1)  //主动回收不重启
                                exit_worker->deleted = 0;
                            else{
                                // 重建该task进程
                                swProcessPool_spawn(exit_worker);
                            }
                        }
                    }
                    //user process
                    if (serv->user_worker_map != NULL)
                        swManager_wait_user_worker(&SwooleGS->event_workers, pid);
                }
            }
            //reload worker
            kill_worker: if (ManagerProcess.reloading == 1)
            {
                // reload finish
                // 如果reload_worker_i索引大于或等于reload_worker_num,说明所有进程都重启了一遍
                // 重置manager_worker_reloading标记和reload_worker_i索引，继续下一次循环
                if (reload_worker_i >= reload_worker_num)
                {
                    ManagerProcess.reloading = 0;
                    reload_worker_i = 0;
                    continue;
                }
                // 调用kill函数杀死reload_worker_i索引指向的worker进程
                ret = kill(reload_workers[reload_worker_i].pid, SIGTERM);
                if (ret < 0)
                    swSysError("kill(%d, SIGTERM) failed.", reload_workers[reload_worker_i].pid);
                // 将reload_worker_i后移一位
                reload_worker_i++;
            }
        }

        sw_free(reload_workers);

        //kill all child process
        for (i = 0; i < serv->worker_num; i++)
        {
            swTrace("[Manager]kill worker processor");
            kill(serv->workers[i].pid, SIGTERM);
        }

        //wait child process
        for (i = 0; i < serv->worker_num; i++)
        {
            if (swWaitpid(serv->workers[i].pid, &status, 0) < 0)
                swSysError("waitpid(%d) failed.", serv->workers[i].pid);
        }

        //kill and wait task process
        if (SwooleG.task_worker_num > 0)
            swProcessPool_shutdown(&SwooleGS->task_workers);

        if (serv->user_worker_map)
        {
            swWorker* user_worker;
            uint64_t key;
            //kill user process
            while (1)
            {
                user_worker = swHashMap_each_int(serv->user_worker_map, &key);
                //hashmap empty
                if (user_worker == NULL)
                    break;
                kill(user_worker->pid, SIGTERM);
            }
            //wait user process
            while (1)
            {
                user_worker = swHashMap_each_int(serv->user_worker_map, &key);
                //hashmap empty
                if (user_worker == NULL)
                    break;
                if (swWaitpid(user_worker->pid, &status, 0) < 0)
                    swSysError("waitpid(%d) failed.", serv->workers[i].pid);
            }
        }

        if (serv->onManagerStop)
            serv->onManagerStop(serv);

        return SW_OK;
}

// 重启一个worker进程
// swManager_spawn_worker创建worker进程，随后通过swWorker_loop进入worker的主循环；
// 当有任务来临时，通过swServer_worker_schedule获取对应的worker，调用swWorker_onTask执行任务。
static pid_t swManager_spawn_worker(swFactory *factory, int worker_id)
{
        pid_t pid;
        int ret;
        pid = fork();
        //fork() failed
        if (pid < 0)
        {
            swWarn("Fork Worker failed. Error: %s [%d]", strerror(errno), errno);
            return SW_ERR;
        }
        //worker child processor
        else if (pid == 0)
        {
            // 进入worker的工作循环,循环结束后推出进程。在父进程中返回worker进程的pid
            ret = swWorker_loop(factory, worker_id);
            exit(ret);
        }
        //parent,add to writer
        else
            return pid;
}

static void swManager_signal_handle(int sig)
{
    switch (sig)
    {
    case SIGTERM:
        SwooleG.running = 0;
        break;
        /**
         * reload all workers
         */
    case SIGUSR1:
        if (ManagerProcess.reloading == 0)
        {
            ManagerProcess.reloading = 1;
            ManagerProcess.reload_event_worker = 1;
        }
        break;
        /**
         * only reload task workers
         */
    case SIGUSR2:
        if (ManagerProcess.reloading == 0)
        {
            ManagerProcess.reloading = 1;
            ManagerProcess.reload_task_worker = 1;
        }
        break;
    default:
        break;
    }
}

int swManager_wait_user_worker(swProcessPool *pool, pid_t pid)
{
    swServer *serv = SwooleG.serv;
    swWorker *exit_worker = swHashMap_find_int(serv->user_worker_map, pid);
    if (exit_worker != NULL)
    {
        return swManager_spawn_user_worker(serv, exit_worker);
    }
    else
    {
        return SW_ERR;
    }
}

pid_t swManager_spawn_user_worker(swServer *serv, swWorker* worker)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        swWarn("Fork Worker failed. Error: %s [%d]", strerror(errno), errno);
        return SW_ERR;
    }
    //child
    else if (pid == 0)
    {
        SwooleWG.id = serv->worker_num + SwooleG.task_worker_num + worker->id;
        serv->onUserWorkerStart(serv, worker);
        exit(0);
    }
    //parent
    else
    {
        if (worker->pid)
        {
            swHashMap_del_int(serv->user_worker_map, worker->pid);
        }
        worker->pid = pid;
        swHashMap_add_int(serv->user_worker_map, pid, worker, NULL);
        return pid;
    }
}
