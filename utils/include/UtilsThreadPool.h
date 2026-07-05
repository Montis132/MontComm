#ifndef _UTIL_THREAD_POOL_H_
#define _UTIL_THREAD_POOL_H_

#include "CommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct _UTIL_TPOOL_MODULE_INIT_ARG
{
    int ThreadPoolSize;
    int Timeout;
    int TaskListMaxLength;
}
UTIL_TPOOL_MODULE_INIT_ARG;

int
Util_TPoolModuleInit(
    UTIL_TPOOL_MODULE_INIT_ARG *InitArg
    );

int
Util_TPoolModuleExit(
    void
    );

int
Util_TPoolAddTask(
    void (*TaskFunc)(void*),
    void* TaskArg
    );

int
Util_TPoolAddTaskAndWait(
    void (*TaskFunc)(void*),
    void* TaskArg,
    int32_t TimeoutSec
    );

int
Util_TPoolModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

void 
Util_TPoolSetTimeout(
    uint32_t Timeout
    );

void
Util_TPoolSetMaxQueueLength(
    int32_t QueueLen
    );

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_THREAD_POOL_H_ */
