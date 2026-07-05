#include "UtilsModuleHealth.h"
#include "UtilsList.h"

#define MODULE_HEALTH_DEFAULT_TIME_INTERVAL                          300 //s

MODULE_HEALTH_REPORT_REGISTER sg_ModuleReprt[UTIL_MODULES_ENUM_MAX] = 
{
    [UTIL_MODULES_ENUM_LOG]       =   {Util_LogModuleCollectStat, MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [UTIL_MODULES_ENUM_MSG]       =   {Util_MsgModuleCollectStat, MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [UTIL_MODULES_ENUM_TPOOL]     =   {Util_TPoolModuleCollectStat, MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [UTIL_MODULES_ENUM_CMDLINE]   =   {Util_CmdLineModuleCollectStat, MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [UTIL_MODULES_ENUM_MHEALTH]   =   {Util_HealthModuleCollectStat, MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [UTIL_MODULES_ENUM_MEM]       =   {Util_MemModuleCollectStat, MODULE_HEALTH_DEFAULT_TIME_INTERVAL},
    [UTIL_MODULES_ENUM_TIMER]     =   {Util_TimerModuleCollectStat, MODULE_HEALTH_DEFAULT_TIME_INTERVAL}
};

typedef struct _HEALTH_MONITOR_LIST_NODE
{
    struct event* Event;
    char Name[UTIL_HEALTH_MONITOR_NAME_MAX_LEN];
    int32_t IntervalS;
    LIST_NODE List;
}
HEALTH_MONITOR_LIST_NODE;

typedef struct _HEALTH_MONITOR
{
    pthread_t ThreadId;
    struct event_base* EventBase;
    BOOL IsRunning;
    LIST_NODE EventList;     // MY_HEALTH_MONITOR_LIST_NODE
    int32_t EventListLen;
    pthread_spinlock_t Lock;
}
HEALTH_MONITOR;

static HEALTH_MONITOR sg_HealthWorker = {.IsRunning = FALSE};
static int32_t sg_HealthModId = UTIL_MEM_MODULE_INVALID_ID;

static void*
_HealthCalloc(
    size_t Size
    )
{
    return Util_MemCalloc(sg_HealthModId, Size);
}

static void
_HealthFree(
    void* Ptr
    )
{
    return Util_MemFree(sg_HealthModId, Ptr);
}

#define HEALTH_MONITOR_KEEPALIVE_INTERVAL                1 // s
static void
_HealthMonitorKeepalive(
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

static void
_HealthModuleStatCommonTemplate(
    evutil_socket_t Fd,
    short Event,
    void *Arg
    )
{
    char logBuff[BUFF_1024 * BUFF_1024] = {0};
    int offset = 0;
    
    UNUSED(Arg);
    UNUSED(Fd);
    UNUSED(Event);

    StatReportCB cb = (StatReportCB)Arg;
    
    if (cb(logBuff, sizeof(logBuff), &offset) == 0 && strlen(logBuff))
    {
        LogDbg("%s", logBuff);
    }
    return ;
}

static void*
_HealthModuleEntry(
    void* Arg
    )
{
    struct timeval tv;
    int loop = 0;
    HEALTH_MONITOR_LIST_NODE *node = NULL;
    
    UNUSED(Arg);

    sg_HealthWorker.EventBase = event_base_new();
    if(!sg_HealthWorker.EventBase)
    {
        LogErr("New event base failed!\n");
        goto CommonReturn;
    }
    // Set event base to be externally referencable and thread-safe
    if (evthread_make_base_notifiable(sg_HealthWorker.EventBase) < 0)
    {
        goto CommonReturn;
    }
    // keepalive
    node = (HEALTH_MONITOR_LIST_NODE*)_HealthCalloc(sizeof(HEALTH_MONITOR_LIST_NODE));
    if (!node)
    {
        goto CommonReturn;
    }
    node->Event = (struct event*)_HealthCalloc(sizeof(struct event));
    if (!node->Event)
    {
        goto CommonReturn;
    }
    tv.tv_sec = HEALTH_MONITOR_KEEPALIVE_INTERVAL;
    tv.tv_usec = 0;
    snprintf(node->Name, sizeof(node->Name), "%s", "Keepalive");
    node->IntervalS = HEALTH_MONITOR_KEEPALIVE_INTERVAL;
    event_assign(node->Event, sg_HealthWorker.EventBase, -1, EV_READ | EV_PERSIST, _HealthMonitorKeepalive, NULL);
    event_add(node->Event, &tv);
    event_active(node->Event, EV_READ, 0);
    LIST_ADD_TAIL(&node->List, &sg_HealthWorker.EventList);
    sg_HealthWorker.EventListLen ++;
    node = NULL;
    
    for(loop = 0; loop < UTIL_MODULES_ENUM_MAX; loop ++)
    {
        if (sg_ModuleReprt[loop].Cb && sg_ModuleReprt[loop].Interval > 0)
        {
            node = (HEALTH_MONITOR_LIST_NODE*)_HealthCalloc(sizeof(HEALTH_MONITOR_LIST_NODE));
            if (!node)
            {
                goto CommonReturn;
            }
            node->Event = (struct event*)_HealthCalloc(sizeof(struct event));
            if (!node->Event)
            {
                goto CommonReturn;
            }
            snprintf(node->Name, sizeof(node->Name), "%s", Util_ModuleNameByEnum(loop));
            node->IntervalS = sg_ModuleReprt[loop].Interval;
            tv.tv_sec = sg_ModuleReprt[loop].Interval;
            tv.tv_usec = 0;
            event_assign(node->Event, sg_HealthWorker.EventBase, -1, EV_PERSIST, _HealthModuleStatCommonTemplate, (void*)sg_ModuleReprt[loop].Cb);
            event_add(node->Event, &tv);
            LIST_ADD_TAIL(&node->List, &sg_HealthWorker.EventList);
            sg_HealthWorker.EventListLen ++;
            node = NULL;
        }
    }

    sg_HealthWorker.IsRunning = TRUE;
    event_base_dispatch(sg_HealthWorker.EventBase);
    
CommonReturn:
    sg_HealthWorker.IsRunning = FALSE;
    if (node)
    {
        _HealthFree(node);
    }
    if (sg_HealthWorker.EventBase)
    {
        event_base_free(sg_HealthWorker.EventBase);
        sg_HealthWorker.EventBase = NULL;
    }
    pthread_exit(NULL);
}

int
Util_HealthMonitorAdd(
    StatReportCB Cb,
    const char* Name,
    int TimeIntervalS
    )
{
    int ret = SUCCESS;
    HEALTH_MONITOR_LIST_NODE *node = NULL;
    struct timeval tv;
    
    node = (HEALTH_MONITOR_LIST_NODE*)_HealthCalloc(sizeof(HEALTH_MONITOR_LIST_NODE));
    if (!node)
    {
        ret = -ENOMEM;
        goto CommonReturn;
    }
    node->Event = (struct event*)_HealthCalloc(sizeof(struct event));
    if (!node->Event)
    {
        ret = -ENOMEM;
        goto CommonReturn;
    }
    tv.tv_sec = TimeIntervalS;
    tv.tv_usec = 0;
    if (Name)
    {
        snprintf(node->Name, sizeof(node->Name), "%s", Name);
    }
    node->IntervalS = TimeIntervalS;
    pthread_spin_lock(&sg_HealthWorker.Lock);
    event_assign(node->Event, sg_HealthWorker.EventBase, -1, EV_PERSIST, _HealthModuleStatCommonTemplate, (void*)Cb);
    event_add(node->Event, &tv);
    LIST_ADD_TAIL(&node->List, &sg_HealthWorker.EventList);
    sg_HealthWorker.EventListLen ++;
    pthread_spin_unlock(&sg_HealthWorker.Lock);

CommonReturn:
    if (ret && node)
    {
        _HealthFree(node);
    }
    return ret;
}

int
Util_HealthModuleExit(
    void
    )
{
    int ret = SUCCESS;
    HEALTH_MONITOR_LIST_NODE *tmp = NULL, *loop = NULL;
    
    if (sg_HealthWorker.IsRunning)
    {
        pthread_spin_lock(&sg_HealthWorker.Lock);
        sg_HealthWorker.EventListLen = 0;
        if (!LIST_IS_EMPTY(&sg_HealthWorker.EventList))
        {
            LIST_FOR_EACH(&sg_HealthWorker.EventList, loop, tmp, HEALTH_MONITOR_LIST_NODE, List)
            {
                LIST_DEL_NODE(&loop->List);
                event_del(loop->Event);
                //event_free(loop->Event);  // no need to free because we use event assign
                _HealthFree(loop->Event);
                _HealthFree(loop);
                loop = NULL;
            }
        }
        pthread_spin_unlock(&sg_HealthWorker.Lock);
        event_base_loopexit(sg_HealthWorker.EventBase, NULL);
        pthread_join(sg_HealthWorker.ThreadId, NULL);
        pthread_spin_destroy(&sg_HealthWorker.Lock);
        ret = Util_MemUnRegister(&sg_HealthModId);
    }

    return ret;
}

int 
Util_HealthModuleInit(
    UTIL_HEALTH_MODULE_INIT_ARG *InitArg
    )
{
    int ret = SUCCESS;
    int sleepIntervalUs = 10;
    int waitTimeUs = sleepIntervalUs * 1000; // 10 ms

    if (sg_HealthWorker.IsRunning)
    {
        goto CommonReturn;
    }
    ret = Util_MemRegister(&sg_HealthModId, "Health");
    if (ret)
    {
        LogErr("Register mem failed!\n");
        goto CommonReturn;
    }

    if (InitArg)
    {
        sg_ModuleReprt[UTIL_MODULES_ENUM_LOG].Interval = InitArg->LogHealthIntervalS;
        sg_ModuleReprt[UTIL_MODULES_ENUM_MSG].Interval = InitArg->MsgHealthIntervalS;
        sg_ModuleReprt[UTIL_MODULES_ENUM_TPOOL].Interval = InitArg->TPoolHealthIntervalS;
        sg_ModuleReprt[UTIL_MODULES_ENUM_CMDLINE].Interval = InitArg->CmdLineHealthIntervalS;
        sg_ModuleReprt[UTIL_MODULES_ENUM_MHEALTH].Interval = InitArg->MHHealthIntervalS;
        sg_ModuleReprt[UTIL_MODULES_ENUM_MEM].Interval = InitArg->MemHealthIntervalS;
        sg_ModuleReprt[UTIL_MODULES_ENUM_TIMER].Interval = InitArg->TimerHealthIntervalS;
    }
    
    pthread_spin_init(&sg_HealthWorker.Lock, PTHREAD_PROCESS_PRIVATE);
    LIST_NODE_INIT(&sg_HealthWorker.EventList);
    sg_HealthWorker.EventListLen = 0;
    ret = pthread_create(&sg_HealthWorker.ThreadId, NULL, _HealthModuleEntry, NULL);
    if (ret)
    {
        goto CommonReturn;
    }

    while(!sg_HealthWorker.IsRunning && waitTimeUs >= 0)
    {
        usleep(sleepIntervalUs);
        waitTimeUs -= sleepIntervalUs;
    }

CommonReturn:
    return ret;
}

int
Util_HealthModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = SUCCESS;
    HEALTH_MONITOR_LIST_NODE *tmp = NULL, *loop = NULL;
    int len = 0;

    len += snprintf(Buff + *Offset + len, BuffMaxLen - *Offset - len, 
            "<%s:(ListLength:%d)", Util_ModuleNameByEnum(UTIL_MODULES_ENUM_MHEALTH), sg_HealthWorker.EventListLen);
    if (!LIST_IS_EMPTY(&sg_HealthWorker.EventList))
    {
        LIST_FOR_EACH(&sg_HealthWorker.EventList, loop, tmp, HEALTH_MONITOR_LIST_NODE, List)
        {
            len += snprintf(Buff + *Offset + len, BuffMaxLen - *Offset - len, 
                "[EventName:%s, EventInterval:%d]", loop->Name, loop->IntervalS);
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

static const char* sg_ModulesName[UTIL_MODULES_ENUM_MAX] = 
{
    [UTIL_MODULES_ENUM_LOG]       =   "Log",
    [UTIL_MODULES_ENUM_MSG]       =   "Msg",
    [UTIL_MODULES_ENUM_TPOOL]     =   "TPool",
    [UTIL_MODULES_ENUM_CMDLINE]   =   "CmdLine",
    [UTIL_MODULES_ENUM_MHEALTH]   =   "MHealth",
    [UTIL_MODULES_ENUM_MEM]       =   "Mem",
    [UTIL_MODULES_ENUM_TIMER]     =   "Timer"
};

const char*
Util_ModuleNameByEnum(
    int Module
    )
{
    return (Module >= 0 && Module < (int)UTIL_MODULES_ENUM_MAX) ? sg_ModulesName[Module] : NULL;
}

