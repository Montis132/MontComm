#ifndef _UTIL_CMD_LINE_H_
#define _UTIL_CMD_LINE_H_

#include "CommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef void (*ExitHandle)(void);
typedef int (*CmdExternalFn)(char*, size_t, char*, size_t); // inBuff-Cmd inBuffLen-Cmdlen outBuff outBuffMaxSize

typedef struct _UTIL_CMD_EXTERNAL_CONT
{
    char Opt[BUFF_64];
    char Help[BUFF_256];
    int Argc;
    CmdExternalFn Cb;
}
UTIL_CMD_EXTERNAL_CONT;

typedef struct _UTIL_CMDLINE_MODULE_INIT_ARG
{
    char RoleName[BUFF_64];
    int Argc;
    char** Argv;
    ExitHandle ExitFunc;
}
UTIL_CMDLINE_MODULE_INIT_ARG;

int
Util_CmdLineModuleInit(
    UTIL_CMDLINE_MODULE_INIT_ARG *InitArg
    );

void
Util_CmdLineModuleExit(
    void
    );

int
Util_CmdLineModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

int
Util_CmdExternalRegister(
    UTIL_CMD_EXTERNAL_CONT Cont
    );

#ifdef __cplusplus
 }
#endif

#endif /* _UTIL_CMD_LINE_H_ */
