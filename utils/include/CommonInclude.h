#ifndef _INCLUDE_H_
#define _INCLUDE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdint.h>
#include <signal.h>
#include <endian.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>

#include <pthread.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/file.h>

#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/util.h"
#include "event2/thread.h"

#include "Errno.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifndef BOOL
#define BOOL                                        unsigned char
#endif
#ifndef TRUE
#define TRUE                                        1
#endif
#ifndef FALSE
#define FALSE                                       0
#endif

#ifndef __in
#define __in
#endif
#ifndef __in_ecout
#define __in_ecout(size)
#endif
#ifndef __out
#define __out
#endif
#ifndef __out_ecout
#define __out_ecout(size)
#endif
#ifndef __inout
#define __inout
#endif
#ifndef __inout_ecout
#define __inout_ecout(size)
#endif
#ifndef UNUSED
#define UNUSED(_p_)                              ((void)(_p_))
#endif
#ifndef MUST_CHECK
#define MUST_CHECK                               __attribute__((warn_unused_result))
#endif
#ifndef LIKELY
#define LIKELY(x)                                __builtin_expect(!!(x), 1)
#endif
#ifndef UNLIKELY
#define UNLIKELY(x)                              __builtin_expect(!!(x), 0)
#endif

#define BUFF_16                                  16
#define BUFF_32                                  32
#define BUFF_64                                  64
#define BUFF_128                                 128
#define BUFF_256                                 256
#define BUFF_512                                 512
#define BUFF_1024                                1024
#define BUFF_2048                                2048
#define BUFF_4096                                4096
#define BUFF_8192                                8192

#define TCP_SERVER_PORT                          15000
#define MAX_CLIENT_NUM_PER_SERVER                128
#define KILL_SIGNAL                              SIGUSR1

#define UATOMIC_INC(addr)                        __sync_fetch_and_add((addr), 1)
#define UATOMIC_DEC(addr)                        __sync_fetch_and_add((addr), -1)

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE_H_ */
