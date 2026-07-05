#ifndef _SERVER_WORKER_H_
#define _SERVER_WORKER_H_
#include <iostream>
#include <map>

#include "SCMsg.pb.h"
#include "UtilsModuleCommon.h"
#include "ServerMsgHandler.h"
#include "CommMngrClient.h"

typedef struct _S_WORKER_INIT_PARAM {
    uint16_t Port;
    std::string Name;
    uint32_t Load;
    std::string WorkPath;
    std::string Sm2PriKeyPwd;
}
S_WORKER_INIT_PARAM;

typedef struct _S_CLIENT_REGISTER_CTX {
    int Status;             // SCMsg::SC_MSG_TYPE_REGISTER_*
    SM2_KEY PubKey;         // get this via CommMngr
    uint8_t ChallengeRandSend[16];
    int TransCipherSuite;   // SCMsg::CipherSuite
    union {
        uint8_t Sm4Key[UTIL_CRYPT_SM4_KEY_LEN];
    }TransKey;
}
S_CLIENT_REGISTER_CTX;

typedef struct _S_CLIENT_NODE {
    int Fd;
    uint32_t ClientId;
    int RecvMsgCnt;
    int ForwardMsgCnt;
    S_CLIENT_REGISTER_CTX RegisterCtx;
    struct epoll_event *RecvEvent;
}
S_CLIENT_NODE;

class ServerWorker{
    friend class ServerMsgHandler;
private:
    S_WORKER_INIT_PARAM InitParam;
    int WorkerFd;
    bool Inited;
    int MemId;
    int EpollFd;
    SM2_KEY Sm2PubKey;
    std::string Sm2PriKeyPath;
    struct epoll_event ListenEvent;
    pthread_t ThreadId;
    std::atomic<uint32_t> ClientCurrentNum;               // size of map is O(log n), so we use num
    pthread_spinlock_t Lock;   // this lock for client map

    ERR_T InitPath(std::string);
    ERR_T InitSm2Key(std::string, std::string);
    ERR_T InitWorkerFd(uint16_t, uint32_t);
    ERR_T InitClientMap();
    ERR_T InitEventBaseAndRun();
    static void* _WorkerThreadFn(void*);
    void ServerAccept(void);
    void ClientRecv(int32_t);
    void EraseClient_NL(int32_t);
    void EraseClient(int32_t);
    ServerMsgHandler *MsgHandler;
public:
    std::map<int, S_CLIENT_NODE*> ClientMap;
    std::unordered_map<uint32_t, int> ClientFdMap;
    CommMngrClient *MngrClient;
    void* Calloc(size_t);
    void Free(void*);
    ServerWorker();
    ~ServerWorker();
    ERR_T Init(S_WORKER_INIT_PARAM);
    void Exit();
    std::string GetStatus();
};

#endif /* _SERVER_WORKER_H_ */