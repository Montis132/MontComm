#ifndef _UTIL_MODULE_TIMER_H_
#define _UTIL_MODULE_TIMER_H_

#include "CommonInclude.h"
#include "UtilsList.h"
#include "UtilsModuleHealth.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef enum _UTIL_TIMER_TYPE
{
    UTIL_TIMER_TYPE_ONE_TIME,
    UTIL_TIMER_TYPE_LOOP,
    
    UTIL_TIMER_TYPE_MAX_UNSPEC
}
UTIL_TIMER_TYPE; 

typedef void (*TimerCB)(evutil_socket_t, short, void*);

typedef struct _UTIL_TIMER_EVENT_NODE
{
    struct event* Event;
    TimerCB Cb;
    void* Arg;
    uint32_t IntervalMs;
    LIST_NODE List;
}
UTIL_TIMER_EVENT_NODE;

typedef UTIL_TIMER_EVENT_NODE* TIMER_HANDLE;

int
Util_TimerModuleExit(
    void
    );

int 
Util_TimerModuleInit(
    void
    );

int
Util_TimerAdd(
    TimerCB Cb,
    uint32_t IntervalMs,
    void* Arg,
    UTIL_TIMER_TYPE TimerType,
    BOOL ActiveNow,
     TIMER_HANDLE *TimerHandle
    );
 
void
Util_TimerDel(
     TIMER_HANDLE *TimerHandle
    );

int
Util_TimerModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

#ifdef __cplusplus
 }
#endif

#endif /* _UTIL_MODULE_TIMER_H_ */
