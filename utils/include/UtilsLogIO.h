#ifndef _UTIL_LOG_IO_H_
#define _UTIL_LOG_IO_H_

#include "CommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

#define ROLE_NAME_MAX_LEN                    128
#define LOF_PATH_MAX_LEN                     256
#define LOG_FILE                             "/tmp/log.log"

#ifdef __GNUC__ 
#define UTIL_FUNC_NAME               __FUNCTION__
#elif defined(_MSC_VER)
#define UTIL_FUNC_NAME               __FUNCSIG__
#else
#define UTIL_FUNC_NAME               __func__
#endif

#define LogInfo(Fmt, ...)               Util_LogPrint(UTIL_LOG_LEVEL_INFO, UTIL_FUNC_NAME, __LINE__, Fmt, ##__VA_ARGS__)
#define LogDbg(Fmt, ...)                Util_LogPrint(UTIL_LOG_LEVEL_DEBUG, UTIL_FUNC_NAME, __LINE__, Fmt, ##__VA_ARGS__)
#define LogWarn(Fmt, ...)               Util_LogPrint(UTIL_LOG_LEVEL_WARNING, UTIL_FUNC_NAME, __LINE__, Fmt, ##__VA_ARGS__)
#define LogErr(Fmt, ...)                Util_LogPrint(UTIL_LOG_LEVEL_ERROR, UTIL_FUNC_NAME, __LINE__, Fmt, ##__VA_ARGS__)

#define LogClassInfo(CLASS, Fmt, ...)   Util_LogPrintWithClass(UTIL_LOG_LEVEL_INFO, UTIL_FUNC_NAME, __LINE__, CLASS, Fmt, ##__VA_ARGS__)
#define LogClassDbg(CLASS, Fmt, ...)    Util_LogPrintWithClass(UTIL_LOG_LEVEL_DEBUG, UTIL_FUNC_NAME, __LINE__, CLASS, Fmt, ##__VA_ARGS__)
#define LogClassWarn(CLASS, Fmt, ...)   Util_LogPrintWithClass(UTIL_LOG_LEVEL_WARNING, UTIL_FUNC_NAME, __LINE__, CLASS, Fmt, ##__VA_ARGS__)
#define LogClassErr(CLASS, Fmt, ...)    Util_LogPrintWithClass(UTIL_LOG_LEVEL_ERROR, UTIL_FUNC_NAME, __LINE__, CLASS, Fmt, ##__VA_ARGS__)

typedef enum _UTIL_LOG_LEVEL
{
    UTIL_LOG_LEVEL_INFO,
    UTIL_LOG_LEVEL_DEBUG,
    UTIL_LOG_LEVEL_WARNING,
    UTIL_LOG_LEVEL_ERROR,
    
    UTIL_LOG_LEVEL_MAX
}
UTIL_LOG_LEVEL;

typedef struct _UTIL_LOG_MODULE_INIT_ARG
{
    char LogFilePath[LOF_PATH_MAX_LEN];
    UTIL_LOG_LEVEL LogLevel;
    int LogMaxSize;     // Mb
    int LogMaxNum;
    char RoleName[ROLE_NAME_MAX_LEN];
}
UTIL_LOG_MODULE_INIT_ARG;

int
Util_LogModuleInit(
    UTIL_LOG_MODULE_INIT_ARG *InitArg
    );

void
Util_LogPrint(
    int level,
    const char* Function,
    int Line,
    const char* Fmt,
    ...
    );
void
Util_LogPrintWithClass(
    int Level,
    const char* Function,
    int Line,
    const char* Class,
    const char* Fmt,
    ...
    );
void
Util_LogModuleExit(
    void
    );

int
Util_LogModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

void
Util_LogSetLevel(
    uint32_t LogLevel
    );

#ifdef __cplusplus
 }
#endif

#endif /* _UTIL_LOG_IO_H_ */
