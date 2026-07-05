#ifndef _COMM_MNGR_CLIENT_H_
#define _COMM_MNGR_CLIENT_H_
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "UtilsModuleCommon.h"

typedef struct _COMM_MNGR_CLIENT_INIT_PARAM {
    std::string ServerAddr;
    std::string TrustedCert;
}
COMM_MNGR_CLIENT_INIT_PARAM;

typedef enum _MNGR_CLIENT_STATS {
    MNGR_CLIENT_STATS_UNSPEC,
    MNGR_CLIENT_STATS_INITED,
    MNGR_CLIENT_STATS_CONNECTED,
    MNGR_CLIENT_STATS_REGISTERED,
    MNGR_CLIENT_STATS_DISCONNECTED,
    MNGR_CLIENT_STATS_EXIT,
    MNGR_CLIENT_STATS_MAX
}
MNGR_CLIENT_STATS;

#define MNGR_C_IS_INITED(_stat_)                   ((_stat_) >= MNGR_CLIENT_STATS_INITED && (_stat_) < MNGR_CLIENT_STATS_EXIT)
#define MNGR_C_SHOULD_EXIT(_stat_)                 ((_stat_) == MNGR_CLIENT_STATS_EXIT)

class CommMngrClient{
private:
    COMM_MNGR_CLIENT_INIT_PARAM InitParam;
    int32_t ClientFd;
    SSL* ClientSSL;
    SSL_CTX* ClientCtx;
    pthread_t ThreadId;
    struct event_base *EventBase;
    struct event *KeepaliveEvent;
    struct event *HealthMonitorEvent;
    struct event *RecvEvent;
    MNGR_CLIENT_STATS State; 

    static void* _MngrCThreadFn(void*);
    static void _RecvMsg(evutil_socket_t ,short ,void *);
    static void _Keepalive(evutil_socket_t ,short ,void *);
    static void _HealthMonitor(evutil_socket_t ,short ,void *);
    void AddRecvEvent(void);
    void RemoveRecvEvent(void);
    ERR_T InitClientFd();
    void CloseClientFd();
    ERR_T ConnectServer();
    ERR_T InitEventBaseAndRun();

public:
    ERR_T Init(COMM_MNGR_CLIENT_INIT_PARAM);
    void Exit();
    ERR_T GetPubKeyAfterGotClientId(uint32_t, SM2_KEY &);
    CommMngrClient();
    ~CommMngrClient();
};

#endif /* _COMM_MNGR_CLIENT_H_ */