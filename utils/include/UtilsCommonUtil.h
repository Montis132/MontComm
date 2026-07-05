#ifndef _UTIL_COMMON_UTIL_H_
#define _UTIL_COMMON_UTIL_H_
#include "CommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

#define UTIL_GET_CPU_USAGE_START                     \
    do { \
        uint64_t _UTIL_TOTAL_CPU_START = 0, _UTIL_TOTAL_CPU_END = 0; \
        uint64_t _UTIL_IDLE_CPU_START = 0, _UTIL_IDLE_CPU_END = 0; \
        (void)Util_GetCpuTime(&_UTIL_TOTAL_CPU_START, &_UTIL_IDLE_CPU_START);

#define UTIL_GET_CPU_USAGE_END(_CPU_USAGE_)                       \
        (void)Util_GetCpuTime(&_UTIL_TOTAL_CPU_END, &_UTIL_IDLE_CPU_END); \
        _CPU_USAGE_ = _UTIL_TOTAL_CPU_END > _UTIL_TOTAL_CPU_START ? \
                    1.0 - (double)(_UTIL_IDLE_CPU_END - _UTIL_IDLE_CPU_START) / \
                                (_UTIL_TOTAL_CPU_END - _UTIL_TOTAL_CPU_START) : 0; \
    }while(0);

void
Util_MakeDaemon(
    void
    );

int
Util_OpenPidFile(
    char* Path
    );

int
Util_IsProcessRunning(
    int Fd
    );

int
Util_SetPidIntoFile(
    int Fd
    );

void 
Util_CloseStdFds(
    void
    );

int
Util_GetCpuTime(
    uint64_t *TotalTime,
    uint64_t *IdleTime
    );

uint64_t 
Util_htonll(
    uint64_t value
    );

uint64_t 
Util_ntohll(
    uint64_t value
    );

void
Util_ChangeCharA2B(
    char* String,
    size_t StringLen,
    char A,
    char B
    );

int
Util_ParseStringToIpv4(
    const char* String,
    size_t StringLen,
    uint32_t *Ip
    );
int
Util_ParseStringToIpv4AndPort(
    const char* String,
    size_t StringLen,
    uint32_t *Ip,
    uint16_t *Port
    );

ERR_T 
Util_GetMemUsage(
    float *Usage
    );

void 
Util_Hexdump(
    const char *Title, 
    unsigned char *Buff, 
    int Length
    );

#ifdef __cplusplus
 }
#endif

#endif /* _UTIL_COMMON_UTIL_H_ */
