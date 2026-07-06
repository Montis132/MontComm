#ifndef _CLIENT_WORKER_H_
#define _CLIENT_WORKER_H_
#include <iostream>
#include <map>
#include <vector>
#include <atomic>

#include "UtilsModuleCommon.h"

typedef enum _C_WORKER_STATS {
    C_WORKER_STATS_UNSPEC,
    C_WORKER_STATS_INITED,
    C_WORKER_STATS_CONNECTED,
    C_WORKER_STATS_REGISTERED,
    C_WORKER_STATS_DISCONNECTED,
    C_WORKER_STATS_EXIT,
    C_WORKER_STATS_MAX
}
C_WORKER_STATS;

#define C_IS_INITED(_stat_)                   ((_stat_) >= C_WORKER_STATS_INITED && (_stat_) < C_WORKER_STATS_EXIT)
#define C_SHOULD_EXIT(_stat_)                 ((_stat_) == C_WORKER_STATS_EXIT)

typedef struct _C_WORKER_SERVER_CONF {
    std::string Addr;
    std::string Name;
    std::string Id;
}
C_WORKER_SERVER_CONF;

typedef struct _C_WORKER_INIT_PARAM {
    uint32_t ClientId;
    std::vector<C_WORKER_SERVER_CONF> Servers;
    std::string WorkPath;
    std::string PriKeyPwd;
    uint32_t CryptoSuite;
}
C_WORKER_INIT_PARAM;

class ClientMsgHandler;

class ClientWorker{
    friend class ClientMsgHandler;
private:
    int WorkerFd;
    pthread_t MsgThreadId;
    pthread_t StateMachineThreadId;
    struct event_base* EventBase;
    struct event* RecvEvent;
    struct event* KeepaliveEvent;
    ClientMsgHandler *MsgHandler;
    uint32_t RegisterRetried;
    int32_t CurrentServerPos;

    ERR_T InitWorkerFd();
    ERR_T ConnectServer();
    void RecreateWorkerFd(void);
    ERR_T RegisterToServer();
    static void _RecvMsg(evutil_socket_t ,short ,void *);
    static void* _MsgThreadFn(void*);
    static void* _StateMachineThreadFn(void*);
    void AddRecvEvent(void);
    void RemoveRecvEvent(void);
    ERR_T InitEventBaseAndRun();
    ERR_T StartStateMachine();
public:
    C_WORKER_STATS State;
    C_WORKER_INIT_PARAM InitParam;
    ClientWorker();
    ~ClientWorker();
    ERR_T Init(C_WORKER_INIT_PARAM);
    void Exit();
    ERR_T SendMsg(uint32_t ClientId, std::string Msg);
    ClientMsgHandler* GetMsgHandler() const { return MsgHandler; }
    C_WORKER_STATS GetState() const { return State; }
};
#endif /* _CLIENT_WORKER_H_ */