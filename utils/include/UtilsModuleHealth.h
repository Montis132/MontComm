#ifndef _UTIL_MODULE_HEALTH_H_
#define _UTIL_MODULE_HEALTH_H_

#include "CommonInclude.h"
#include "UtilsLogIO.h"
#include "UtilsMsg.h"
#include "UtilsThreadPool.h"
#include "UtilsMem.h"
#include "UtilsTimer.h"
#include "UtilsCmdLine.h"

#ifdef __cplusplus
extern "C"{
#endif

#define UTIL_HEALTH_MONITOR_NAME_MAX_LEN                                        BUFF_64

typedef enum _UTIL_MODULES_ENUM
{
    UTIL_MODULES_ENUM_LOG,
    UTIL_MODULES_ENUM_MSG,
    UTIL_MODULES_ENUM_TPOOL,
    UTIL_MODULES_ENUM_CMDLINE,
    UTIL_MODULES_ENUM_MHEALTH,
    UTIL_MODULES_ENUM_MEM,
    UTIL_MODULES_ENUM_TIMER,
    
    UTIL_MODULES_ENUM_MAX
}
UTIL_MODULES_ENUM;

typedef int (*StatReportCB)(char*, int, int*);

typedef struct _MODULE_HEALTH_REPORT_REGISTER
{
    StatReportCB Cb;
    int Interval;
}
MODULE_HEALTH_REPORT_REGISTER;

typedef struct _UTIL_HEALTH_MODULE_INIT_ARG
{
    int LogHealthIntervalS;
    int MsgHealthIntervalS;
    int TPoolHealthIntervalS;
    int CmdLineHealthIntervalS;
    int MHHealthIntervalS;
    int MemHealthIntervalS;
    int TimerHealthIntervalS;
}
UTIL_HEALTH_MODULE_INIT_ARG;

extern MODULE_HEALTH_REPORT_REGISTER sg_ModuleReprt[UTIL_MODULES_ENUM_MAX];

int
Util_HealthModuleExit(
    void
    );

int 
Util_HealthModuleInit(
    UTIL_HEALTH_MODULE_INIT_ARG *InitArg
    );

int
Util_HealthMonitorAdd(
    StatReportCB Cb,
    const char* Name,
    int TimeIntervalS
    );

int
Util_HealthModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );
 
const char*
Util_ModuleNameByEnum(
    int Module
    );

#ifdef __cplusplus
 }
#endif

#endif /* _UTIL_MODULE_HEALTH_H_ */
