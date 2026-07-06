#include "UtilsThreadPool.h"
#include "UtilsModuleHealth.h"
#include "UtilsLogIO.h"
#include "UtilsList.h"
#include "UtilsCommonUtil.h"

#define TASK_TIME_OUT_DEFAULT_VAL                            5 //seconds
#define THREAD_POOL_SIZE                                        5
#define THREAD_POOL_MAX_TASK_LENGTH                             1024

typedef enum _TPOOL_TASK_STATUS
{
    TPOOL_TASK_STATUS_UNSPEC,
    TPOOL_TASK_STATUS_INIT,
    TPOOL_TASK_STATUS_WORKING,
    TPOOL_TASK_STATUS_TIMEOUT
}
TPOOL_TASK_STATUS;

typedef struct _TPOOL_TASK
{
    void (*TaskFunc)(void*);
    void* TaskArg;
    BOOL HasTimeOut;
    pthread_mutex_t *TaskLock;
    pthread_cond_t *TaskCond;
    TPOOL_TASK_STATUS TaskStat;
    LIST_NODE List;
}
TPOOL_TASK;

typedef struct _THREAD_POOL_STATS
{
    int TaskAdded;
    int TaskSucceed;
    int TaskFailed;
}
THREAD_POOL_STATS;

typedef struct _THREAD_POOL{
    pthread_mutex_t Lock;
    pthread_cond_t Cond;
    pthread_t *Threads;
    volatile int CurrentThreadNum;
    LIST_NODE TaskListHead;      // MY_TEST_THREAD_TASK
    int TaskListLength;
    int TaskMaxLength;
    int TaskDefaultTimeout;
    volatile BOOL Exit;
}
THREAD_POOL;

//__thread 
THREAD_POOL* sg_ThreadPool = NULL;
static THREAD_POOL_STATS sg_ThreadPoolStats = {.TaskAdded = 0, .TaskSucceed = 0, .TaskFailed = 0};
//__thread 
BOOL sg_TPoolModuleInited = FALSE;
static int sg_TPoolMemId = UTIL_MEM_MODULE_INVALID_ID;

static void*
_TPoolCalloc(
    size_t Size
    )
{
    return Util_MemCalloc(sg_TPoolMemId, Size);
}

static void
_TPoolFree(
    void* Ptr
    )
{
    return Util_MemFree(sg_TPoolMemId, Ptr);
}

static void* 
_TPoolProcFn(
    void* arg
    )
{
    THREAD_POOL* threadPool = (THREAD_POOL*)arg;
    TPOOL_TASK* loop = NULL, *tmp = NULL;
    LIST_NODE listHeadTmp;
    BOOL hasTask = FALSE;
    //LogInfo("Thread worker %lu entering...", syscall(SYS_gettid));
    
    while (!threadPool->Exit) 
    {
        hasTask = FALSE;
        pthread_mutex_lock(&threadPool->Lock);
        UATOMIC_INC(&threadPool->CurrentThreadNum);
        do{
            if (threadPool->Exit)
            {
                pthread_mutex_unlock(&threadPool->Lock);
                goto CommonReturn;
            }
            if (!LIST_IS_EMPTY(&threadPool->TaskListHead))
            {
                LIST_HEAD_COPY(&listHeadTmp, &threadPool->TaskListHead);
                LIST_NODE_INIT(&threadPool->TaskListHead);
                threadPool->TaskListLength = 0;
                hasTask = TRUE;
                break;
            }
        }while(pthread_cond_wait(&threadPool->Cond, &threadPool->Lock) == 0);
        UATOMIC_DEC(&threadPool->CurrentThreadNum);
        pthread_mutex_unlock(&threadPool->Lock);

        if (!hasTask)
        {
            continue;
        }
        
        
        LIST_FOR_EACH(&listHeadTmp, loop, tmp, TPOOL_TASK, List)
        {
            if (loop && loop->TaskFunc)
            {
                if (loop->HasTimeOut && loop->TaskStat == TPOOL_TASK_STATUS_TIMEOUT)
                {
                    pthread_mutex_destroy(loop->TaskLock);
                    pthread_cond_destroy(loop->TaskCond);
                    _TPoolFree(loop->TaskLock);
                    _TPoolFree(loop->TaskCond);
                }
                else
                {
                    loop->TaskFunc(loop->TaskArg);
                    if (loop->HasTimeOut)
                    {
                        if (loop->TaskStat == TPOOL_TASK_STATUS_TIMEOUT)
                        {
                            pthread_mutex_destroy(loop->TaskLock);
                            pthread_cond_destroy(loop->TaskCond);
                            _TPoolFree(loop->TaskLock);
                            _TPoolFree(loop->TaskCond);
                        }
                        else
                        {
                            pthread_mutex_lock(loop->TaskLock);
                            pthread_cond_signal(loop->TaskCond);
                            pthread_mutex_unlock(loop->TaskLock);
                        }
                    }
                    UATOMIC_INC(&sg_ThreadPoolStats.TaskSucceed);
                }
                LIST_DEL_NODE(&loop->List);
                _TPoolFree(loop);
                loop = NULL;
            }
        }
    }

CommonReturn:
    UATOMIC_DEC(&threadPool->CurrentThreadNum);
    //LogInfo("Thread worker %lu exit.", syscall(SYS_gettid));
    pthread_exit(NULL);
}

int
Util_TPoolModuleInit(
    UTIL_TPOOL_MODULE_INIT_ARG *InitArg
    )
{
    int loop = 0;
    int ret = SUCCESS;
    pthread_attr_t attr;
    int sleepIntervalMs = 1;
    int waitTimeMs = sleepIntervalMs * 10; // 10 ms

    if (sg_TPoolModuleInited)
    {
        goto CommonReturn;
    }

    if (!InitArg || InitArg->ThreadPoolSize <= 0)
    {
        ret = -EINVAL;
        goto CommonReturn;
    }

    ret = Util_MemRegister(&sg_TPoolMemId, "TPool");
    if (ret)
    {
        LogErr("Mem register failed! ret %d", ret);
        goto CommonReturn;
    }
    
    sg_ThreadPool = (THREAD_POOL*)_TPoolCalloc(sizeof(THREAD_POOL));
    if (!sg_ThreadPool)
    {
        ret = -ENOMEM;
        goto CommonReturn;
    }
    
    sg_ThreadPool->TaskMaxLength = InitArg->TaskListMaxLength ? 
        InitArg->TaskListMaxLength : THREAD_POOL_MAX_TASK_LENGTH;
    
    pthread_mutex_init(&sg_ThreadPool->Lock, NULL);
    pthread_cond_init(&sg_ThreadPool->Cond, NULL);
    sg_ThreadPool->Exit = FALSE;
    sg_ThreadPool->CurrentThreadNum = 0;
    LIST_NODE_INIT(&sg_ThreadPool->TaskListHead);
    sg_ThreadPool->TaskListLength = 0;
    
    ret = pthread_attr_init(&attr);
    assert(!ret);
    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    assert(!ret);
    
    pthread_mutex_lock(&sg_ThreadPool->Lock);
    {
        sg_ThreadPool->Threads = (pthread_t*)_TPoolCalloc(sizeof(pthread_t) * InitArg->ThreadPoolSize);
        for (loop = 0; loop < InitArg->ThreadPoolSize; loop ++)
        {
            ret = pthread_create(&(sg_ThreadPool->Threads[loop]), &attr, _TPoolProcFn, (void*)sg_ThreadPool);
            assert(!ret);
        }
        ret = pthread_attr_destroy(&attr);
        assert(!ret);
    }
    pthread_mutex_unlock(&sg_ThreadPool->Lock);

    sg_ThreadPool->TaskDefaultTimeout = InitArg->Timeout ? InitArg->Timeout : TASK_TIME_OUT_DEFAULT_VAL;
    sg_TPoolModuleInited = TRUE;

    // to make sure workers are ready
    while(waitTimeMs >= 0)
    {
        pthread_mutex_lock(&sg_ThreadPool->Lock);
        if (sg_ThreadPool->CurrentThreadNum >= InitArg->ThreadPoolSize)
        {
            pthread_mutex_unlock(&sg_ThreadPool->Lock);
            break;
        }
        pthread_mutex_unlock(&sg_ThreadPool->Lock);
        usleep(sleepIntervalMs * 1000);
        waitTimeMs -= sleepIntervalMs;
    };
    
CommonReturn:
    return ret;
}

int
Util_TPoolModuleExit(
    void
    )
{
    int ret = SUCCESS;

    if (sg_TPoolModuleInited)
    {
        sg_ThreadPool->Exit = TRUE;
        pthread_mutex_lock(&(sg_ThreadPool->Lock));
        pthread_cond_broadcast(&(sg_ThreadPool->Cond));
        pthread_mutex_unlock(&(sg_ThreadPool->Lock));

        pthread_mutex_destroy(&(sg_ThreadPool->Lock));
        pthread_cond_destroy(&(sg_ThreadPool->Cond));
        _TPoolFree(sg_ThreadPool->Threads);
        _TPoolFree(sg_ThreadPool);
        sg_ThreadPool = NULL;
        sg_TPoolModuleInited = FALSE;
        ret = Util_MemUnRegister(&sg_TPoolMemId);
    }

    return ret;
}

// async api, TaskArg is a in arg
int
Util_TPoolAddTask(
    void (*TaskFunc)(void*),
     void* TaskArg
    )
{
    int ret = SUCCESS;
    TPOOL_TASK *node = NULL;
    
    if (!sg_TPoolModuleInited)
    {
        ret = -EINVAL;
        goto CommonReturn;
    }
    
    node = (TPOOL_TASK*)_TPoolCalloc(sizeof(TPOOL_TASK));
    if (!node) 
    {
        ret = -ENOMEM;
        LogErr("Apply mem failed!");
        goto CommonReturn;
    }
    node->HasTimeOut = FALSE;
    node->TaskArg = TaskArg;
    node->TaskFunc = TaskFunc;
    LIST_NODE_INIT(&node->List);
    
    pthread_mutex_lock(&sg_ThreadPool->Lock);
    {
        if (sg_ThreadPool->TaskListLength >= sg_ThreadPool->TaskMaxLength)
        {
            ret = -EBUSY;
            LogErr("%d task in schedule, drop!", sg_ThreadPool->TaskListLength);
            pthread_cond_signal(&sg_ThreadPool->Cond);
            pthread_mutex_unlock(&sg_ThreadPool->Lock);
            _TPoolFree(node);
            goto CommonReturn;
        }
        LIST_ADD_TAIL(&node->List, &sg_ThreadPool->TaskListHead);
        sg_ThreadPool->TaskListLength ++;
        pthread_cond_signal(&sg_ThreadPool->Cond);
        UATOMIC_INC(&sg_ThreadPoolStats.TaskAdded);
    }
    pthread_mutex_unlock(&sg_ThreadPool->Lock);
    
CommonReturn:
    return ret;
}

// sync api, you can care about TaskArg inout
// when TimeoutSec > 0, use it; otherwise use sg_TPoolTaskTimeout
int
Util_TPoolAddTaskAndWait(
    void (*TaskFunc)(void*),
     void* TaskArg,
    int32_t TimeoutSec
    )
{
    int ret = SUCCESS;
    struct timespec ts = {0, 0};
    BOOL taskAdded = FALSE;
    pthread_mutex_t *taskLock;
    pthread_cond_t *taskCond;
    pthread_condattr_t taskAttr;
    BOOL taskInited = FALSE;
    TPOOL_TASK *node = NULL;
    
    if (!sg_TPoolModuleInited || (sg_ThreadPool->TaskDefaultTimeout <= 0 && TimeoutSec <= 0))
    {
        ret = -EINVAL;
        goto CommonReturn;
    }
    taskLock = (pthread_mutex_t*)_TPoolCalloc(sizeof(pthread_mutex_t));
    taskCond = (pthread_cond_t*)_TPoolCalloc(sizeof(pthread_cond_t));
    node = (TPOOL_TASK*)_TPoolCalloc(sizeof(TPOOL_TASK));
    if (!taskLock || !taskCond || !node)
    {
        ret = -ENOMEM;
        LogErr("Apply mem failed!");
        goto CommonReturn;
    }
    
    pthread_condattr_init(&taskAttr);
    pthread_condattr_setclock(&taskAttr, CLOCK_MONOTONIC);
    pthread_mutex_init(taskLock, NULL);
    pthread_cond_init(taskCond, &taskAttr);
    pthread_condattr_destroy(&taskAttr);
    taskInited = TRUE;
    
    node->HasTimeOut = TRUE;
    node->TaskArg = TaskArg;
    node->TaskFunc = TaskFunc;
    node->TaskCond = taskCond;
    node->TaskLock = taskLock;
    node->TaskStat = TPOOL_TASK_STATUS_INIT;
    
    pthread_mutex_lock(&sg_ThreadPool->Lock);
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != SUCCESS)
    {
        pthread_mutex_unlock(&sg_ThreadPool->Lock);
        ret = -EIO;
        LogErr("Get time failed!");
        goto CommonReturn;
    }
    LIST_ADD_TAIL(&node->List, &sg_ThreadPool->TaskListHead);
    sg_ThreadPool->TaskListLength ++;
    taskAdded = TRUE;
    pthread_mutex_lock(taskLock);
    pthread_cond_signal(&sg_ThreadPool->Cond);
    UATOMIC_INC(&sg_ThreadPoolStats.TaskAdded);
    pthread_mutex_unlock(&sg_ThreadPool->Lock);

    if (taskAdded)
    {
        if (TimeoutSec > 0)
        {
            ts.tv_sec += TimeoutSec;
            LogInfo("Add with timeout %d", TimeoutSec);
        }
        else
        {
            ts.tv_sec += sg_ThreadPool->TaskDefaultTimeout;
            LogInfo("Add with timeout %d", sg_ThreadPool->TaskDefaultTimeout);
        }
        ret = -pthread_cond_timedwait(taskCond, taskLock, &ts);
    }
    if (-ETIMEDOUT == ret)
    {
        LogErr("Task wait timeout!");
        node->TaskStat = TPOOL_TASK_STATUS_TIMEOUT;
    }
    pthread_mutex_unlock(taskLock);
    
CommonReturn:
    if (ret != SUCCESS)
        UATOMIC_INC(&sg_ThreadPoolStats.TaskFailed);
    if (taskInited && (!taskAdded || ret != -ETIMEDOUT))
    {
        pthread_mutex_destroy(taskLock);
        pthread_cond_destroy(taskCond);
        _TPoolFree(taskLock);
        _TPoolFree(taskCond);
    }
    return ret;
}

int
Util_TPoolModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = SUCCESS;
    int len = 0;
    
    if (!sg_TPoolModuleInited)
    {
        // protect sg_ThreadPool
        goto CommonReturn;
    }
    len = snprintf(Buff + *Offset, BuffMaxLen - *Offset, 
        "<%s:[TaskAdded=%d, TaskSucceed=%d, TaskFailed=%d, CurrentThreadNum=%d, TaskListLength=%d]>",
            Util_ModuleNameByEnum(UTIL_MODULES_ENUM_TPOOL), 
            sg_ThreadPoolStats.TaskAdded, sg_ThreadPoolStats.TaskSucceed, sg_ThreadPoolStats.TaskFailed,
            sg_ThreadPool->CurrentThreadNum, sg_ThreadPool->TaskListLength);
    if (len < 0 || len >= BuffMaxLen - *Offset)
    {
        ret = -ENOMEM;
        LogErr("Too long Msg!");
        goto CommonReturn;
    }
    else
    {
        *Offset += len;
    }
CommonReturn:
    return ret;
}

void
Util_TPoolSetTimeout(
    uint32_t Timeout
    )
{
    if (sg_TPoolModuleInited && sg_ThreadPool)
    {
        pthread_mutex_lock(&sg_ThreadPool->Lock);
        sg_ThreadPool->TaskDefaultTimeout = Timeout;
        pthread_mutex_unlock(&sg_ThreadPool->Lock);
    }
}

void
Util_TPoolSetMaxQueueLength(
    int32_t QueueLen
    )
{
    if (sg_TPoolModuleInited && sg_ThreadPool)
    {
        pthread_mutex_lock(&sg_ThreadPool->Lock);
        sg_ThreadPool->TaskMaxLength = QueueLen;
        pthread_mutex_unlock(&sg_ThreadPool->Lock);
    }
}

UTIL_TPOOL_HANDLE
Util_TPoolGetHandle(
    void
    )
{
    if (!sg_TPoolModuleInited) {
        return NULL;
    }
    return (UTIL_TPOOL_HANDLE)sg_ThreadPool;
}

int
Util_TPoolAddTaskByHandle(
    UTIL_TPOOL_HANDLE Handle,
    void (*TaskFunc)(void*),
    void* TaskArg
    )
{
    int ret = SUCCESS;
    THREAD_POOL* pool = (THREAD_POOL*)Handle;
    TPOOL_TASK *node = NULL;

    if (!pool || !TaskFunc) {
        ret = -EINVAL;
        goto CommonReturn;
    }

    node = (TPOOL_TASK*)_TPoolCalloc(sizeof(TPOOL_TASK));
    if (!node) {
        ret = -ENOMEM;
        goto CommonReturn;
    }
    node->HasTimeOut = FALSE;
    node->TaskArg = TaskArg;
    node->TaskFunc = TaskFunc;
    LIST_NODE_INIT(&node->List);

    pthread_mutex_lock(&pool->Lock);
    if (pool->TaskListLength >= pool->TaskMaxLength) {
        ret = -EBUSY;
        pthread_cond_signal(&pool->Cond);
        pthread_mutex_unlock(&pool->Lock);
        _TPoolFree(node);
        goto CommonReturn;
    }
    LIST_ADD_TAIL(&node->List, &pool->TaskListHead);
    pool->TaskListLength++;
    pthread_cond_signal(&pool->Cond);
    UATOMIC_INC(&sg_ThreadPoolStats.TaskAdded);
    pthread_mutex_unlock(&pool->Lock);

CommonReturn:
    return ret;
}

int
Util_TPoolAddTaskAndWaitByHandle(
    UTIL_TPOOL_HANDLE Handle,
    void (*TaskFunc)(void*),
    void* TaskArg,
    int32_t TimeoutSec
    )
{
    int ret = SUCCESS;
    THREAD_POOL* pool = (THREAD_POOL*)Handle;
    struct timespec ts = {0, 0};
    BOOL taskAdded = FALSE;
    pthread_mutex_t *taskLock;
    pthread_cond_t *taskCond;
    pthread_condattr_t taskAttr;
    BOOL taskInited = FALSE;
    TPOOL_TASK *node = NULL;

    if (!pool || !TaskFunc) {
        ret = -EINVAL;
        goto CommonReturn;
    }

    taskLock = (pthread_mutex_t*)_TPoolCalloc(sizeof(pthread_mutex_t));
    taskCond = (pthread_cond_t*)_TPoolCalloc(sizeof(pthread_cond_t));
    node = (TPOOL_TASK*)_TPoolCalloc(sizeof(TPOOL_TASK));
    if (!taskLock || !taskCond || !node) {
        ret = -ENOMEM;
        goto CommonReturn;
    }

    pthread_condattr_init(&taskAttr);
    pthread_condattr_setclock(&taskAttr, CLOCK_MONOTONIC);
    pthread_mutex_init(taskLock, NULL);
    pthread_cond_init(taskCond, &taskAttr);
    pthread_condattr_destroy(&taskAttr);
    taskInited = TRUE;

    node->HasTimeOut = TRUE;
    node->TaskArg = TaskArg;
    node->TaskFunc = TaskFunc;
    node->TaskCond = taskCond;
    node->TaskLock = taskLock;
    node->TaskStat = TPOOL_TASK_STATUS_INIT;

    pthread_mutex_lock(&pool->Lock);
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != SUCCESS) {
        pthread_mutex_unlock(&pool->Lock);
        ret = -EIO;
        goto CommonReturn;
    }
    LIST_ADD_TAIL(&node->List, &pool->TaskListHead);
    pool->TaskListLength++;
    taskAdded = TRUE;
    pthread_mutex_lock(taskLock);
    pthread_cond_signal(&pool->Cond);
    UATOMIC_INC(&sg_ThreadPoolStats.TaskAdded);
    pthread_mutex_unlock(&pool->Lock);

    if (taskAdded) {
        ts.tv_sec += (TimeoutSec > 0) ? TimeoutSec : pool->TaskDefaultTimeout;
        ret = -pthread_cond_timedwait(taskCond, taskLock, &ts);
    }
    if (-ETIMEDOUT == ret) {
        node->TaskStat = TPOOL_TASK_STATUS_TIMEOUT;
    }
    pthread_mutex_unlock(taskLock);

CommonReturn:
    if (ret != SUCCESS)
        UATOMIC_INC(&sg_ThreadPoolStats.TaskFailed);
    if (taskInited && (!taskAdded || ret != -ETIMEDOUT)) {
        pthread_mutex_destroy(taskLock);
        pthread_cond_destroy(taskCond);
        _TPoolFree(taskLock);
        _TPoolFree(taskCond);
    }
    return ret;
}

int
Util_TPoolDestroyHandle(
    UTIL_TPOOL_HANDLE Handle
    )
{
    THREAD_POOL* pool = (THREAD_POOL*)Handle;
    if (!pool) {
        return -EINVAL;
    }
    pool->Exit = TRUE;
    pthread_mutex_lock(&(pool->Lock));
    pthread_cond_broadcast(&(pool->Cond));
    pthread_mutex_unlock(&(pool->Lock));

    pthread_mutex_destroy(&(pool->Lock));
    pthread_cond_destroy(&(pool->Cond));
    _TPoolFree(pool->Threads);
    _TPoolFree(pool);
    if (pool == sg_ThreadPool) {
        sg_ThreadPool = NULL;
        sg_TPoolModuleInited = FALSE;
    }
    return SUCCESS;
}

