#include "UnitTest.h"
#include "UtilsThreadPool.h"
#include "UtilsCommonUtil.h"
#include "UtilsMem.h"
#include "UtilsLogIO.h"

#define UT_TPOOL_TEST_VAL                                           132
#define UT_TPOOL_TEST_TIMEOUT_VAL                                   133
#define UT_TPOOL_MAX_CPU_USAGE                                      0.1     // 10%

typedef struct _UT_TPOOL_TASK_ARG
{
    int Value;
    void* Ptr;
}
UT_TPOOL_TASK_ARG;

static BOOL sg_UT_TPool_TaskErrHapped = FALSE;

static int
_UT_TPool_PreInit(
    void
    )
{
    return Util_MemModuleInit();
}

static int
_UT_TPool_FinExit(
    void
    )
{
    return Util_MemModuleExit();
}

static void
_UT_TPool_AddTaskCb(
    void* Arg
    )
{
    UT_TPOOL_TASK_ARG *taskArg = (UT_TPOOL_TASK_ARG *)Arg;

    if ((taskArg->Value != UT_TPOOL_TEST_VAL && taskArg->Value != UT_TPOOL_TEST_TIMEOUT_VAL) || taskArg->Ptr != Arg)
    {
        sg_UT_TPool_TaskErrHapped = TRUE;
        UTLog("Invalid arg, value:%d arg:%p taskarg:%p\n", taskArg->Value, taskArg, taskArg->Ptr);
    }
    if (taskArg->Value == UT_TPOOL_TEST_TIMEOUT_VAL)
    {
        UTLog("Waiting ...\n");
        usleep(1500 * 1000);
    }
    Util_Free(taskArg);
    UTLog("_TPool_AddTaskCb success\n");
}

static int
_UT_TPool_InitExit(
    void
    )
{
    int ret = SUCCESS;
    UTIL_TPOOL_MODULE_INIT_ARG initArg = {.ThreadPoolSize = 256, .Timeout = 5, .TaskListMaxLength = 1024};

    Util_LogSetLevel(1);
    
    ret = Util_TPoolModuleInit(&initArg);
    if (ret)
    {
        UTLog("Init fail\n");
        goto CommonReturn;
    }

CommonReturn:
    if (Util_TPoolModuleExit())
    {
        ret = -EINVAL;
    }
    if (!Util_MemLeakSafetyCheck())
    {
        ret = -EINVAL;
    }
    Util_LogSetLevel(0);
    return ret;
}

static int
_UT_TPool_ForwardT(
    void
    )
{
    int ret = SUCCESS;
    UT_TPOOL_TASK_ARG *taskArg = NULL;
    double cpuUsage = 0;
    UTIL_TPOOL_MODULE_INIT_ARG initArg = {.ThreadPoolSize = 3, .Timeout = 5, .TaskListMaxLength = 1024};
    
UTIL_GET_CPU_USAGE_START
{
    ret = Util_TPoolModuleInit(&initArg);
    if (ret)
    {
        UTLog("Init fail\n");
        goto CommonReturn;
    }

    taskArg = (UT_TPOOL_TASK_ARG*)Util_Calloc(sizeof(UT_TPOOL_TASK_ARG));
    if (!taskArg)
    {
        ret = -ENOMEM;
        goto CommonReturn;
    }
    taskArg->Value = UT_TPOOL_TEST_VAL;
    taskArg->Ptr = (void*)taskArg;
    ret = Util_TPoolAddTask(_UT_TPool_AddTaskCb, (void*)taskArg);
    if (ret)
    {
        UTLog("Add fail\n");
        goto CommonReturn;
    }

    taskArg = (UT_TPOOL_TASK_ARG*)Util_Calloc(sizeof(UT_TPOOL_TASK_ARG));
    if (!taskArg)
    {
        ret = -ENOMEM;
        goto CommonReturn;
    }
    taskArg->Value = UT_TPOOL_TEST_VAL;
    taskArg->Ptr = (void*)taskArg;
    ret = Util_TPoolAddTaskAndWait(_UT_TPool_AddTaskCb, (void*)taskArg, 5);
    if (ret)
    {
        UTLog("Add fail\n");
        goto CommonReturn;
    }
    
    taskArg = (UT_TPOOL_TASK_ARG*)Util_Calloc(sizeof(UT_TPOOL_TASK_ARG));
    if (!taskArg)
    {
        ret = -ENOMEM;
        goto CommonReturn;
    }
    taskArg->Value = UT_TPOOL_TEST_VAL;
    taskArg->Ptr = (void*)taskArg;
    ret = Util_TPoolAddTask(_UT_TPool_AddTaskCb, (void*)taskArg);
    if (ret)
    {
        UTLog("Add fail\n");
        goto CommonReturn;
    }
    
    taskArg = (UT_TPOOL_TASK_ARG*)Util_Calloc(sizeof(UT_TPOOL_TASK_ARG));
    if (!taskArg)
    {
        ret = -ENOMEM;
        goto CommonReturn;
    }
    taskArg->Value = UT_TPOOL_TEST_TIMEOUT_VAL;
    taskArg->Ptr = (void*)taskArg;
    ret = Util_TPoolAddTaskAndWait(_UT_TPool_AddTaskCb, (void*)taskArg, 1);
    if (ret != -ETIMEDOUT)
    {
        UTLog("wait fail, ret %d\n", ret);
        ret = -EIO;
        goto CommonReturn;
    }
    else
    {
        ret = SUCCESS;
    }
    sleep(1);
    usleep(100 * 100);
}
UTIL_GET_CPU_USAGE_END(cpuUsage);
    UTLog("Cpu usage %lf\n", cpuUsage);

    if (cpuUsage > UT_TPOOL_MAX_CPU_USAGE)
    {
        ret = -E2BIG;
    }
    
CommonReturn:
    if (Util_TPoolModuleExit())
    {
        ret = -EINVAL;
    }
    if (!Util_MemLeakSafetyCheck())
    {
        ret = -EINVAL;
    }
    return ret;
}

int main()
{
    assert(SUCCESS == _UT_TPool_PreInit());
    assert(SUCCESS == _UT_TPool_InitExit());
    assert(SUCCESS == _UT_TPool_FinExit());
    
    assert(SUCCESS == _UT_TPool_PreInit());
    assert(SUCCESS == _UT_TPool_ForwardT());
    assert(SUCCESS == _UT_TPool_FinExit());
    
    assert(FALSE == sg_UT_TPool_TaskErrHapped);

    return 0;
}
