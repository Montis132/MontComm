#include "UtilsTimer.h"
#include "UtilsLogIO.h"
#include "UtilsMem.h"

typedef struct _TIMER_WORKER
{
    pthread_t ThreadId;
    struct event_base* EventBase;
    BOOL IsRunning;
    LIST_NODE EventList;     // MY_TIMER_EVENT_NODE
    int32_t EventListLen;
    pthread_mutex_t Lock;
}
TIMER_WORKER;

static TIMER_WORKER sg_TimerWorker = {.IsRunning = FALSE};
static int sg_TimerMemId = UTIL_MEM_MODULE_INVALID_ID;

static void*
_TimerCalloc(
    size_t Size
    )
{
    return Util_MemCalloc(sg_TimerMemId, Size);
}

static void
_TimerFree(
    void* Ptr
    )
{
    return Util_MemFree(sg_TimerMemId, Ptr);
}

#define TIMER_KEEPALIVE_INTERVAL                             1 // s
static void
_TimerWorkerKeepalive(
    evutil_socket_t Fd,
    short Event,
    void *Arg
    )
{
    UNUSED(Fd);
    UNUSED(Event);
    UNUSED(Arg);
    return ;
}

static void*
_TimerModuleEntry(
    void* Arg
    )
{
    struct timeval tv;
    UTIL_TIMER_EVENT_NODE *node = NULL;
    
    UNUSED(Arg);

    sg_TimerWorker.EventBase = event_base_new();
    if(!sg_TimerWorker.EventBase)
    {
        LogErr("New event base failed!\n");
        goto CommonReturn;
    }
    sg_TimerWorker.EventListLen = 0;
    // Set event base to be externally referencable and thread-safe
    if (evthread_make_base_notifiable(sg_TimerWorker.EventBase) < 0)
    {
        goto CommonReturn;
    }
    // keep alive
    node = (UTIL_TIMER_EVENT_NODE*)_TimerCalloc(sizeof(UTIL_TIMER_EVENT_NODE));
    if (!node)
    {
        goto CommonReturn;
    }
    node->Event = (struct event*)_TimerCalloc(sizeof(struct event));
    if (!node->Event)
    {
        goto CommonReturn;
    }
    node->Cb = _TimerWorkerKeepalive;
    node->IntervalMs = TIMER_KEEPALIVE_INTERVAL * 1000;
    tv.tv_sec = TIMER_KEEPALIVE_INTERVAL;
    tv.tv_usec = 0;
    pthread_mutex_lock(&sg_TimerWorker.Lock);
    event_assign(node->Event, sg_TimerWorker.EventBase, -1, EV_READ | EV_PERSIST, _TimerWorkerKeepalive, NULL);
    event_add(node->Event, &tv);
    event_active(node->Event, EV_READ, 0);
    LIST_ADD_TAIL(&node->List, &sg_TimerWorker.EventList);
    sg_TimerWorker.EventListLen ++;
    node = NULL;
    pthread_mutex_unlock(&sg_TimerWorker.Lock);

    sg_TimerWorker.IsRunning = TRUE;
    event_base_dispatch(sg_TimerWorker.EventBase);
    
CommonReturn:
    sg_TimerWorker.IsRunning = FALSE;
    if (node)
    {
        _TimerFree(node);
    }
    if (sg_TimerWorker.EventBase)
    {
        event_base_free(sg_TimerWorker.EventBase);
        sg_TimerWorker.EventBase = NULL;
    }
    pthread_exit(NULL);
}

int
Util_TimerModuleExit(
    void
    )
{
    int ret = SUCCESS;
    UTIL_TIMER_EVENT_NODE *tmp = NULL, *loop = NULL;
    
    if (sg_TimerWorker.IsRunning)
    {
        pthread_mutex_lock(&sg_TimerWorker.Lock);
        sg_TimerWorker.EventListLen = 0;
        if (!LIST_IS_EMPTY(&sg_TimerWorker.EventList))
        {
            LIST_FOR_EACH(&sg_TimerWorker.EventList, loop, tmp, UTIL_TIMER_EVENT_NODE, List)
            {
                LIST_DEL_NODE(&loop->List);
                event_del(loop->Event);
                //event_free(loop->Event);  // no need to free because we use event assign
                _TimerFree(loop->Event);
                _TimerFree(loop);
                loop = NULL;
            }
        }
        pthread_mutex_unlock(&sg_TimerWorker.Lock);
        event_base_loopexit(sg_TimerWorker.EventBase, NULL);
        pthread_join(sg_TimerWorker.ThreadId, NULL);
        pthread_mutex_destroy(&sg_TimerWorker.Lock);
        ret = Util_MemUnRegister(&sg_TimerMemId);
    }

    return ret;
}

int 
Util_TimerModuleInit(
    void
    )
{
    int ret = SUCCESS;
    int sleepIntervalUs = 10;
    int waitTimeUs = sleepIntervalUs * 1000; // 10 ms

    if (sg_TimerWorker.IsRunning)
    {
        goto CommonReturn;
    }

    ret = Util_MemRegister(&sg_TimerMemId, "Timer");
    if (ret)
    {
        goto CommonReturn;
    }

    pthread_mutex_init(&sg_TimerWorker.Lock, NULL);
    LIST_NODE_INIT(&sg_TimerWorker.EventList);
    sg_TimerWorker.EventListLen = 0;
    ret = pthread_create(&sg_TimerWorker.ThreadId, NULL, _TimerModuleEntry, NULL);
    if (ret)
    {
        LogErr("Create thread ret %d:%s", ret , StrErr(ret));
        goto CommonReturn;
    }

    while(!sg_TimerWorker.IsRunning && waitTimeUs >= 0)
    {
        usleep(sleepIntervalUs);
        waitTimeUs -= sleepIntervalUs;
    }

CommonReturn:
    return ret;
}

int
Util_TimerAdd(
    TimerCB Cb,
    uint32_t IntervalMs,
    void* Arg,
    UTIL_TIMER_TYPE TimerType,
    BOOL ActiveNow,
    __out TIMER_HANDLE *TimerHandle
    )
{
    int ret = SUCCESS;
    UTIL_TIMER_EVENT_NODE *node = NULL;
    struct timeval tv;

    if (!sg_TimerWorker.IsRunning || 
        (TimerType != UTIL_TIMER_TYPE_ONE_TIME && TimerType != UTIL_TIMER_TYPE_LOOP) || 
        !Cb ||
        (!TimerHandle && TimerType == UTIL_TIMER_TYPE_LOOP))
    {
        ret = -EINVAL;
        goto CommonReturn;
    }

    tv.tv_sec = IntervalMs / 1000;
    tv.tv_usec = (IntervalMs % 1000) * 1000;
    if (TimerType == UTIL_TIMER_TYPE_LOOP)
    {
        node = (UTIL_TIMER_EVENT_NODE*)_TimerCalloc(sizeof(UTIL_TIMER_EVENT_NODE));
        if (!node)
        {
            goto CommonReturn;
        }
        node->Event = (struct event*)_TimerCalloc(sizeof(struct event));
        if (!node->Event)
        {
            goto CommonReturn;
        }
        node->Cb = Cb;
        node->Arg = Arg;
        node->IntervalMs = IntervalMs;
        pthread_mutex_lock(&sg_TimerWorker.Lock);
        event_assign(node->Event, sg_TimerWorker.EventBase, -1, EV_READ | EV_PERSIST, Cb, Arg);
        event_add(node->Event, &tv);
        LIST_ADD_TAIL(&node->List, &sg_TimerWorker.EventList);
        sg_TimerWorker.EventListLen ++;
        pthread_mutex_unlock(&sg_TimerWorker.Lock);
        if (ActiveNow) {
            event_active(node->Event, EV_READ, 0);
        }
        *TimerHandle = node;
    }
    else
    {
        event_base_once(sg_TimerWorker.EventBase, -1, EV_TIMEOUT, Cb, Arg, &tv);
    }
    
CommonReturn:
    if (ret && node)
    {
        _TimerFree(node);
    }
    return ret;
}

void
Util_TimerDel(
    __inout TIMER_HANDLE *TimerHandle
    )
{
    if (TimerHandle && *TimerHandle)
    {
        pthread_mutex_lock(&sg_TimerWorker.Lock);
        LIST_DEL_NODE(&(*TimerHandle)->List);
        sg_TimerWorker.EventListLen --;
        event_del((*TimerHandle)->Event);
        _TimerFree((*TimerHandle)->Event);
        _TimerFree((*TimerHandle));
        pthread_mutex_unlock(&sg_TimerWorker.Lock);
        (*TimerHandle) = NULL;
    }
}

int
Util_TimerModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = SUCCESS;
    UTIL_TIMER_EVENT_NODE *tmp = NULL, *loop = NULL;
    int len = 0;

    len += snprintf(Buff + *Offset + len, BuffMaxLen - *Offset - len, 
            "<%s:(ListLength:%d)", Util_ModuleNameByEnum(UTIL_MODULES_ENUM_TIMER), sg_TimerWorker.EventListLen);
    if (!LIST_IS_EMPTY(&sg_TimerWorker.EventList))
    {
        LIST_FOR_EACH(&sg_TimerWorker.EventList, loop, tmp, UTIL_TIMER_EVENT_NODE, List)
        {
            len += snprintf(Buff + *Offset + len, BuffMaxLen - *Offset - len, 
                "[Handle:%p, Cb:%p, Arg:%p, IntervalMs:%u]", loop, loop->Cb, loop->Arg, loop->IntervalMs);
            if (len < 0 || len >= BuffMaxLen - *Offset - len)
            {
                ret = -ENOMEM;
                LogErr("Too long Msg!");
                goto CommonReturn;
            }
        }
        
    }
    
    len += snprintf(Buff + *Offset + len, BuffMaxLen - *Offset - len, ">");
    
CommonReturn:
    *Offset += len;
    return ret;
}

