#include "UnitTest.h"
#include "UtilsModuleHealth.h"
#include "UtilsCommonUtil.h"

#define UT_HEALTH_CB_TIMEVAL                                1 //s
#define UT_HEALTH_WAIT_TIME                                 3 //s
#define UT_HEALTH_NAME                                      "UT_HEALTH"

static int sg_UT_HealthCbCalled = 0;

int
_UT_Health_CheckFunc(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    )
{
    int ret = SUCCESS;
    int len = 0;
    double cpuUsage = 0;
    
    sg_UT_HealthCbCalled ++;
    
UTIL_GET_CPU_USAGE_START
{
    usleep(10);
}
UTIL_GET_CPU_USAGE_END(cpuUsage);
    
    len = snprintf(Buff + *Offset, BuffMaxLen - *Offset, 
                        "<%s:cpuUsage=%lf>","_UT_Health_CheckFunc", cpuUsage);
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


static int
_UT_Health_PreInit(
    void
    )
{
    return Util_MemModuleInit();
}

static int
_UT_Health_FinExit(
    void
    )
{
    return Util_MemModuleExit();
}

static int
_UT_Health_ForwardT(
    void
    )
{
    int ret = SUCCESS;
    
    ret = Util_HealthModuleInit(NULL);
    if (ret)
    {
        UTLog("Health init failed!\n");
        goto CommonReturn;
    }

    ret = Util_HealthMonitorAdd(_UT_Health_CheckFunc, UT_HEALTH_NAME, UT_HEALTH_CB_TIMEVAL);
    if (ret)
    {
        UTLog("Health Add failed!\n");
        goto CommonReturn;
    }
    sleep(UT_HEALTH_WAIT_TIME);
    if (sg_UT_HealthCbCalled < (UT_HEALTH_WAIT_TIME - 1) / UT_HEALTH_CB_TIMEVAL)
    {
        ret = -EIO;
        UTLog("Health func called %d.\n", sg_UT_HealthCbCalled);
        goto CommonReturn;
    }
    
CommonReturn:
    if (Util_HealthModuleExit())
    {
        ret = -EINVAL;
    }
    return ret;
}


int main()
{
    assert(SUCCESS == _UT_Health_PreInit());
    assert(SUCCESS == _UT_Health_ForwardT());
    assert(SUCCESS == _UT_Health_FinExit());

    return 0;
}

