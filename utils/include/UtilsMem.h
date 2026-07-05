#ifndef _UTIL_MEM_H_
#define _UTIL_MEM_H_

#include "CommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

#define UTIL_MEM_MODULE_INVALID_ID                                   -1

/*  After the initialization process, you can utilize the "register" and "unregister" functions to 
 *  manage separate memids independently. Later, you can use "MemCalloc" and "MemFree" functions 
 *  for unified memid management. However, for the sake of convenience, I will handle the management 
 *  with a unified memid, using MyCalloc and MyFree.
 */

int
Util_MemModuleInit(
    void
    );

int
Util_MemModuleExit(
    void
    );

int
Util_MemModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

int
Util_MemRegister(
    int *MemId,
    char *Name
    );

int
Util_MemUnRegister(
    int* MemId
    );


void*
Util_MemCalloc(
    int MemId,
    size_t Size
    );

void
Util_MemFree(
    int MemId,
    void* Ptr
    );

void*
Util_Calloc(
    size_t Size
    );

void
Util_Free(
    void* Ptr
    );

BOOL
Util_MemLeakSafetyCheck(
    void
    );

BOOL
Util_MemLeakSafetyCheckWithId(
    int MemId
    );

#ifdef __cplusplus
 }
#endif

#endif /* _UTIL_MEM_H_ */
