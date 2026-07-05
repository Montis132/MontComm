#ifndef _UTIL_MODULE_COMMON_H_
#define _UTIL_MODULE_COMMON_H_

#include "CommonInclude.h"
#include "UtilsLogIO.h"
#include "UtilsMsg.h"
#include "UtilsThreadPool.h"
#include "UtilsCmdLine.h"
#include "UtilsModuleHealth.h"
#include "UtilsCrypto.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct _UTIL_MODULES_INIT_PARAM
{
    UTIL_CMDLINE_MODULE_INIT_ARG *CmdLineArg;
    UTIL_LOG_MODULE_INIT_ARG *LogArg;
    BOOL InitMsgModule;
    UTIL_HEALTH_MODULE_INIT_ARG *HealthArg;
    UTIL_TPOOL_MODULE_INIT_ARG *TPoolArg;
    BOOL InitTimerModule;
}
UTIL_MODULES_INIT_PARAM;

int
Util_ModuleCommonInit(
    UTIL_MODULES_INIT_PARAM ModuleInitParam
    );

void
Util_ModuleCommonExit(
    void
    );

#ifdef __cplusplus
 }
#endif

#endif /* _UTIL_MODULE_COMMON_H_ */
