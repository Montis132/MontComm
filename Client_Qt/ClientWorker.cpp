#include <curl/curl.h>

#include <cstdint>
#include "SCMsg.h"
#include "ClientWorker.h"
#include "UtilsCommonUtil.h"
#include "ClientMsgBussiness.h"

using namespace std;

static string sg_CStatString[C_WORKER_STATS_MAX] = {
    [C_WORKER_STATS_UNSPEC]          =   "Unknown",
    [C_WORKER_STATS_INITED]          =   "Inited",
    [C_WORKER_STATS_CONNECTED]       =   "Connected",
    [C_WORKER_STATS_REGISTERED]      =   "Active",
    [C_WORKER_STATS_DISCONNECTED]    =   "Disconnected",
    [C_WORKER_STATS_EXIT]            =   "Exited",
};

ERR_T ClientWorker::ConnectServer(void) {
    ERR_T ret = SUCCESS;
    bool connectSuccess = false;
    struct sockaddr_in serverAddr;
    uint32_t ip = 0;
    uint16_t port = 0;

    serverAddr.sin_family = AF_INET;
    for(size_t loop = CurrentServerPos >= 0 ? CurrentServerPos + 1 : rand() % InitParam.Servers.size(); 
        loop < InitParam.Servers.size(); 
        loop ++) {
        ret = Util_ParseStringToIpv4AndPort(InitParam.Servers[loop].Addr.c_str(), InitParam.Servers[loop].Addr.length(), &ip, &port);
        if (ret < SUCCESS || ip == 0 || port == 0) {
            LogErr("Parse %s failed!", InitParam.Servers[loop].Addr.c_str());
            continue;
        }
        serverAddr.sin_addr.s_addr = htonl(ip);
        serverAddr.sin_port = htons(port);
        if (connect(WorkerFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            LogErr("Connect to %s(%u:%u) failed, errno %d:%s, try next one", InitParam.Servers[loop].Addr.c_str(), ip, port, errno, StrErr(errno));
            continue;
        } else {
            connectSuccess = true;
            CurrentServerPos = loop;
            break;
        }
    }

    if (!connectSuccess) {
        LogErr("Connect failed!");
        ret = -ENOTCONN;
        goto CommRet;
    } else {
        LogDbg("Connect success! using %s", InitParam.Servers[CurrentServerPos].Addr.c_str());
        State = C_WORKER_STATS_CONNECTED;
        RegisterRetried = 0;
        AddRecvEvent();
    }

CommRet:
    return ret;
}

ERR_T ClientWorker::InitWorkerFd(void) {
    ERR_T ret = SUCCESS;
    struct timeval tv;
    int32_t reuseable = 1; // set port reuseable when fd closed
    // socket
    WorkerFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (WorkerFd < 0) {
        LogErr("Socket failed!");
        ret = -EBADFD; 
        goto CommRet;
    }
    // reuse
    if (setsockopt(WorkerFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable)) == -1) {
        LogErr("setsockopt SO_REUSEADDR failed!");
        ret = -EIO; 
        goto CommErr;
    }
    // timeout
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(WorkerFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set recv timeout failed\n");
        goto CommErr;
    }
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(WorkerFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        printf("Set send timeout failed\n");
        goto CommErr;
    }
    goto CommRet;
    
CommErr:
    close(WorkerFd);
    WorkerFd = -1;
CommRet:
    return ret;
} 

ERR_T ClientWorker::RegisterToServer(void) {
    ERR_T ret = SUCCESS;
    SCMsg::MsgPayload msgPayload;
    UTIL_Q_MSG *qMsg = NULL;
    // init payload
    MsgHandler->ProtoInitMsg(this, msgPayload);
    // check register send
    if (RegisterRetried >= 3) {
        RemoveRecvEvent();
        RecreateWorkerFd();
        State = C_WORKER_STATS_DISCONNECTED;
        goto CommRet;
    }
    // create registerproto
    MsgHandler->CreateRegisterProtoMsg(this, msgPayload.msgBase);
    // msg pre send
    MsgHandler->ProtoPreSend(msgPayload);
    // send msg
    uint8_t encodeBuf[8192];
    size_t encodedSize = SCMsg::MsgPayloadEncodeToBuf(msgPayload, encodeBuf, sizeof(encodeBuf));
    qMsg = Util_NewSendQMsg(encodedSize);
    if (!qMsg) {
        ret = -ENOMEM;
        LogErr("Get msg mem failed!");
        goto CommRet;
    }
    memcpy(qMsg->Cont.VarLenCont, encodeBuf, encodedSize);
    ret = Util_SendQMsg(WorkerFd, qMsg);
    if (ret < SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", SCMsg::MsgPayloadToString(msgPayload).c_str());
    RegisterRetried ++;

CommRet:
    if (qMsg) 
        Util_FreeSendQMsg(qMsg);
    MsgHandler->ProtoRelease(msgPayload);
    return ret;
}

void*
ClientWorker::_MsgThreadFn(void *Arg) {
    ClientWorker *worker = (ClientWorker*)Arg;
    
    event_base_dispatch(worker->EventBase);
    // break at here
    LogDbg("event base loop break here!");
    event_base_free(worker->EventBase);
    worker->EventBase = NULL;

    return NULL;
}

void ClientWorker::AddRecvEvent(void) {
    if (!RecvEvent && EventBase) {
        RecvEvent = event_new(EventBase, WorkerFd, EV_READ|EV_PERSIST, _RecvMsg, (void*)this);
        event_add(RecvEvent, NULL);
        LogDbg("Adding recv event into base......");
    }
}

void ClientWorker::RemoveRecvEvent(void) {
    if (RecvEvent) {
        event_del(RecvEvent);
        event_free(RecvEvent);
        RecvEvent = NULL;
        LogDbg("Recv event deleted from base.");
    }
}

void ClientWorker::RecreateWorkerFd(void) {
    close(WorkerFd);
    WorkerFd = -1;
    (void)InitWorkerFd();
}

void ClientWorker::_RecvMsg(evutil_socket_t Fd, short Event, void *Arg) {
    ClientWorker *worker = (ClientWorker*)Arg;
    ERR_T ret = SUCCESS;
    UTIL_Q_MSG *recvMsg = NULL;
    SCMsg::MsgPayload msgPayload;
    UNUSED(Event);

    recvMsg = Util_NewRecvQMsg();
    if (!recvMsg) {
        LogErr("Apply failed!");
        return ;
    }
    ret = Util_RecvQMsg(Fd, recvMsg);
    if (ret < 0) {
        LogErr("recv failed!");
        worker->RemoveRecvEvent();
        worker->RecreateWorkerFd();
        worker->State = C_WORKER_STATS_DISCONNECTED;
        goto CommRet;
    }
    if (!SCMsg::MsgPayloadDecodeFromBuf(msgPayload, (uint8_t*)recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen)) {
        LogErr("parse form array failed!");
        goto CommRet;
    }
    LogInfo("Recv msg: %s", SCMsg::MsgPayloadToString(msgPayload).c_str());
    ret = worker->MsgHandler->DispatchMsg(worker, msgPayload);
    if (ret < 0) {
        LogErr("dispatch msg failed! ret %d", ret);
        goto CommRet;
    }

CommRet:
    if (recvMsg)
        Util_FreeRecvQMsg(recvMsg);
    return ;
}

void*
ClientWorker::_StateMachineThreadFn(void *Arg) {
    ClientWorker *worker = (ClientWorker*)Arg;
    ERR_T ret = SUCCESS;
    
    while(C_IS_INITED(worker->State)) {
        switch (worker->State) {
            case C_WORKER_STATS_INITED:
            case C_WORKER_STATS_DISCONNECTED:
                ret = worker->ConnectServer();
                if (ret < SUCCESS) {
                    LogErr("Connect failed! ret %d", ret);
                }
                ret = worker->RegisterToServer();
                if (ret < SUCCESS) {
                    LogErr("Send register msg failed! ret %d", ret);
                }
                break;
            case C_WORKER_STATS_CONNECTED:
                ret = worker->RegisterToServer();
                if (ret < SUCCESS) {
                    LogErr("Send register msg failed! ret %d", ret);
                }
                LogInfo("Worker in stat <%s>.", sg_CStatString[worker->State].c_str());
                break;
            case C_WORKER_STATS_REGISTERED:
                break;
            case C_WORKER_STATS_EXIT:
            default:
                LogErr("In stat %d, exiting stat machine!", worker->State);
                return NULL;
        }
        if (worker->State != C_WORKER_STATS_REGISTERED)
            LogInfo("Worker in stat <%s>.", sg_CStatString[worker->State].c_str());
        sleep(3);
    }

    return NULL;
}

static void _Keepalive(evutil_socket_t Fd, short Event, void *Arg) {
    UNUSED(Fd);
    UNUSED(Event);
    UNUSED(Arg);
}

ERR_T ClientWorker::InitEventBaseAndRun(void) {
    ERR_T ret = SUCCESS;
    pthread_attr_t attr;
    BOOL initPthreadAttr = FALSE;
    int result = 0;
    struct timeval tv;

    if (EventBase != NULL) {
        goto CommRet;
    }

    EventBase = event_base_new();
    if (!EventBase) {
        ret = -ENOMEM;
        LogErr("Create worker event base failed!");
        goto CommRet;
    }
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    KeepaliveEvent = event_new(EventBase, -1, EV_READ|EV_PERSIST, _Keepalive, NULL);
    if (!KeepaliveEvent) {
        ret = -ENOMEM;
        LogErr("Create keepalive event base failed!");
        goto CommRet;
    }
    event_add(KeepaliveEvent, &tv);
    event_active(KeepaliveEvent, 0, EV_READ);   // Must be actively activated once, otherwise it will not run

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    initPthreadAttr = TRUE;

    result = pthread_create(&MsgThreadId, &attr, _MsgThreadFn, (void*)this);
    if (0 != result) {
        ret = -EIO;
        LogErr("Create worker thread failed! errno %d", errno);
        event_free(KeepaliveEvent);
        KeepaliveEvent = NULL;
        event_base_free(EventBase);
        EventBase = NULL;
        goto CommRet;
    }

CommRet:
    if (initPthreadAttr)
        pthread_attr_destroy(&attr);
    return ret;
}

ERR_T ClientWorker::StartStateMachine(void) {
    ERR_T ret = SUCCESS;
    pthread_attr_t attr;
    BOOL initPthreadAttr = FALSE;
    int result = 0;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    initPthreadAttr = TRUE;

    result = pthread_create(&StateMachineThreadId, &attr, _StateMachineThreadFn, (void*)this);
    if (result != 0) {
        ret = -EIO;
        LogErr("create thread failed! ret %d errno %d\n", result, errno);
    }

    if (initPthreadAttr)
        pthread_attr_destroy(&attr);
    return ret;
}

ERR_T ClientWorker::Init(C_WORKER_INIT_PARAM InitParam){
    ERR_T ret = SUCCESS;

    if (C_IS_INITED(State)) {
        goto CommRet;
    }
    
    ClientWorker::InitParam = InitParam;
    MsgHandler = new ClientMsgHandler;
    if (MsgHandler == NULL) {
        LogErr("Init MsgHandler failed! ret %d", ret);
        goto InitErr;
    }
    ret = InitWorkerFd();
    if (ret < SUCCESS) {
        LogErr("Init fd failed! ret %d", ret);
        goto InitErr;
    }
    ret = InitEventBaseAndRun();
    if (ret < SUCCESS) {
        LogErr("Enter work fn failed! ret %d", ret);
        goto InitErr;
    }
    State = C_WORKER_STATS_INITED;
    ret = StartStateMachine();
    if (ret < SUCCESS) {
        LogErr("Enter work fn failed! ret %d", ret);
        goto InitErr;
    }
    goto CommRet;

InitErr:
    Exit();
CommRet:
    return ret;
}

void ClientWorker::Exit(void) {
    if (C_IS_INITED(State)) {
        if (WorkerFd >= 0) {
            close(WorkerFd);
            WorkerFd = -1;
        }
        if (MsgHandler) {
            delete MsgHandler;
            MsgHandler = NULL;
        }
        if (EventBase) {
            RemoveRecvEvent();
            event_base_loopbreak(EventBase);
            if (KeepaliveEvent) {
                event_del(KeepaliveEvent);
                event_free(KeepaliveEvent);
                KeepaliveEvent = NULL;
            }
        }
        State = C_WORKER_STATS_EXIT;
    }
}

ERR_T ClientWorker::SendMsg(uint32_t ClientId, std::string Msg) {
    SCMsg::MsgPayload sendMsgPayload;
    ERR_T ret = SUCCESS;
    uint8_t msgpackBuf[4096];
    size_t msgpackLen = 0;
    UTIL_Q_MSG *sendMsg = NULL;

    if (State < C_WORKER_STATS_REGISTERED) {
        LogErr("Not registered, cannot send");
        ret = -EPERM;
        goto CommRet;
    }

    MsgHandler->ProtoInitMsg(this, sendMsgPayload);
    sendMsgPayload.msgBase.msgType = SCMsg::SC_MSG_TYPE_MSG_TRANS_C_2_S;
    sendMsgPayload.msgBase.transMsg.from.clientId = InitParam.ClientId;
    sendMsgPayload.msgBase.transMsg.to.clientId = ClientId;
    sendMsgPayload.msgBase.transMsg.msg = Msg;
    MsgHandler->ProtoPreSend(sendMsgPayload);
    msgpackLen = SCMsg::MsgPayloadEncodeToBuf(sendMsgPayload, msgpackBuf, sizeof(msgpackBuf));
    sendMsg = Util_NewSendQMsg(msgpackLen);
    if (!sendMsg) {
        ret = -ENOMEM;
        LogErr("Get msg mem failed!");
        goto CommRet;
    }
    memcpy(sendMsg->Cont.VarLenCont, msgpackBuf, msgpackLen);
    ret = Util_SendQMsg(WorkerFd, sendMsg);
    if (ret < SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", SCMsg::MsgPayloadToString(sendMsgPayload).c_str());

CommRet:
    if (sendMsg)
        Util_FreeSendQMsg(sendMsg);
    MsgHandler->ProtoRelease(sendMsgPayload);
    return ret;
}

ClientWorker::ClientWorker() :
    WorkerFd(-1), State(C_WORKER_STATS_UNSPEC), EventBase(NULL), RecvEvent(NULL), MsgHandler(NULL), RegisterRetried(0), CurrentServerPos(-1){}

ClientWorker::~ClientWorker() {}
