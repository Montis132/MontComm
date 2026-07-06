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
    std::string CommMngrUrl;
}
C_WORKER_INIT_PARAM;

class ClientMsgHandler;

typedef struct _S_CLIENT_REGISTER_CTX {
    int Status;             // SCMsg::SC_MSG_TYPE_REGISTER_*
    SM2_KEY PubKey;         // get this via CommMngr
    uint8_t ChallengeRandSend[16];
    int TransCipherSuite;   // SCMsg::CipherSuite SC_CIPHER_SUITE_SM4/SC_CIPHER_SUITE_NONE
    union {
        uint8_t Sm4Key[UTIL_CRYPT_SM4_KEY_LEN];
    }TransKey;
    uint32_t RegisterRetried;
}
S_CLIENT_REGISTER_CTX;

class ClientWorker{
    friend class ClientMsgHandler;
private:
    int WorkerFd;
    C_WORKER_STATS State;
    pthread_t MsgThreadId;
    TIMER_HANDLE StatMachineTimer;
    C_WORKER_INIT_PARAM InitParam;
    struct event_base* EventBase;
    struct event* RecvEvent;
    struct event* KeepaliveEvent;
    ClientMsgHandler *MsgHandler;
    S_CLIENT_REGISTER_CTX RegisterCtx;
    SM2_KEY PubKey;
    std::string PriKeyPath;
    int32_t CurrentServerPos;
    int MemId;

    ERR_T InitPath(std::string);
    ERR_T InitSm2Key(std::string, std::string);
    ERR_T InitWorkerFd();
    ERR_T GetServerInfo();
    ERR_T ConnectServer();
    void RecreateWorkerFd(void);
    ERR_T RegisterToServer();
    static void _RecvMsg(evutil_socket_t ,short ,void *);
    static void* _MsgThreadFn(void*);
    void AddRecvEvent(void);
    void RemoveRecvEvent(void);
    ERR_T InitEventBaseAndRun();
    ERR_T StartStateMachine();
    static void _StateMachineFn(evutil_socket_t Fd, short Ev, void* Arg);
public:
    ClientWorker();
    ~ClientWorker();
    ERR_T SendMsg(uint32_t, std::string);
    ERR_T Init(C_WORKER_INIT_PARAM);
    void Exit();
};
#endif /* _CLIENT_WORKER_H_ */