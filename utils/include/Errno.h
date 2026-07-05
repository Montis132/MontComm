#ifndef _ERRNO_H_
#define _ERRNO_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <errno.h>
#include <string.h>

typedef int ERR_T;

#define SUCCESS              0

typedef enum _ERR_NO_ENUM
{
    ERR_NO_START             = 255,
    ERR_EXIT_WITH_SUCCESS    = 256,
    ERR_PEER_CLOSED          = 257,
    ERR_MEM_LEAK             = 258,
    
    ERR_NO_ENUM_MAX 
}
ERR_NO_ENUM;

static const char* sg_TestErrnoStr[ERR_NO_ENUM_MAX - ERR_NO_START] = 
{
    [ERR_NO_START - ERR_NO_START]             = "unused error: errno start",
    [ERR_EXIT_WITH_SUCCESS - ERR_NO_START]    = "exit with success: not an error",
    [ERR_PEER_CLOSED - ERR_NO_START]          = "peer closed",
    [ERR_MEM_LEAK - ERR_NO_START]             = "mem leak"
};

static inline const char*
StrErr(
    int Errno
    )
{
    int absErr = Errno > 0 ? Errno : -Errno;
    
    return absErr < ERR_NO_START ? strerror(absErr) : 
            (absErr < ERR_NO_ENUM_MAX ? sg_TestErrnoStr[absErr - ERR_NO_START] : "UnknownErr");
}

#ifdef __cplusplus
 }
#endif

#endif /* _ERRNO_H_ */
