#include <curl/curl.h>
#include <filesystem>

#include "SCMsg.pb.h"
#include "ClientWorker.h"
#include "UtilsCommonUtil.h"
#include "ClientMsgHandler.h"

using namespace std;

static string sg_CStatString[C_WORKER_STATS_MAX] = {
    [C_WORKER_STATS_UNSPEC]          =   "Unknown",
    [C_WORKER_STATS_INITED]          =   "Inited",
    [C_WORKER_STATS_CONNECTED]       =   "Connected",
    [C_WORKER_STATS_REGISTERED]      =   "Active",
    [C_WORKER_STATS_DISCONNECTED]    =   "Disconnected",
    [C_WORKER_STATS_EXIT]            =   "Exited",
};

#define CLIENT_WORKER_PUBKEY_FILE                            "C_SM2_Pub.pem"
#define CLIENT_WORKER_PRIKEY_FILE                            "C_SM2_Pri.pem"

ERR_T 
ClientWorker::InitPath(std::string WorkPath) {
    ERR_T ret = SUCCESS;
    std::filesystem::path workDir = WorkPath;
    
    try {
        if (!std::filesystem::exists(workDir)) {
            LogInfo("Directory does not exist, creating...");
            if (std::filesystem::create_directories(workDir)) {
                LogInfo("Directory created successfully.");
            } else {
                LogErr("Failed to create directory.");
                return -EIO;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LogErr("Failed to create directory. %s", e.what());
        return -EIO;
    }

    return ret;
}

ERR_T 
ClientWorker::InitSm2Key(std::string CryptoPath, std::string PriKeyPwd) {
    ERR_T ret = SUCCESS;
    std::filesystem::path cryptoDir = CryptoPath;
    std::filesystem::path pubkeyFile = cryptoDir / CLIENT_WORKER_PUBKEY_FILE;
    std::filesystem::path prikeyFile = cryptoDir / CLIENT_WORKER_PRIKEY_FILE;
    
    try {
        if (!std::filesystem::exists(pubkeyFile) || !std::filesystem::exists(prikeyFile)) {
            ret = Util_CryptSm2KeyGenAndExport(pubkeyFile.string().c_str(), prikeyFile.string().c_str(), PriKeyPwd.c_str());
            if (ret) {
                LogErr("GenKey failed! ret %d", ret);
                return ret;
            } else {
                LogDbg("Genkey success!");
                PriKeyPath = prikeyFile.string();
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LogErr("Failed to check file existence. %s", e.what());
        return -EIO;
    }

    ret = Util_CryptSm2ImportPubKey(pubkeyFile.string().c_str(), &PubKey);
    if (ret) {
        LogErr("Import key failed! ret %d", ret);
        return ret;
    }
    ClientWorker::PriKeyPath = prikeyFile.string();
    Util_Hexdump("Local-Sm2PubKey", (uint8_t*)&PubKey.public_key, sizeof(PubKey.public_key));

    return ret;
}

ERR_T ClientWorker::GetServerInfo(void) {
    ERR_T ret = SUCCESS;
    ret =  Util_CryptSm2ImportPubKey("worker1/S_SM2_Pub.pem", &RegisterCtx.PubKey);
    
    Util_Hexdump("Server-Sm2PubKey", (uint8_t*)&RegisterCtx.PubKey.public_key, sizeof(RegisterCtx.PubKey.public_key));
    return ret;
}

ERR_T ClientWorker::ConnectServer(void) {
    ERR_T ret = SUCCESS;
    bool connectSuccess = false;
    struct sockaddr_in serverAddr;
    uint32_t ip = 0;
    uint16_t port = 0;

    serverAddr.sin_family = AF_INET;
    ret = GetServerInfo();
    if (ret) {
        LogErr("Get ServerInfo failed!");
        goto CommRet;
    }
    for(//size_t loop = CurrentServerPos >= 0 ? CurrentServerPos + 1 : rand() % InitParam.Servers.size(); 
        size_t loop = 0;
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
        RegisterCtx.Status = 0;
        RegisterCtx.RegisterRetried = 0;
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
    std::string serializedData;
    // init payload
    MsgHandler->ProtoInitMsg(msgPayload);
    // check register send
    if (RegisterCtx.RegisterRetried >= 10) {
        RemoveRecvEvent();
        RecreateWorkerFd();
        State = C_WORKER_STATS_DISCONNECTED;
        LogErr("Retried too much, set dissconenct.");
        goto CommRet;
    }
    // create registerproto
    ret = MsgHandler->CreateRegisterProtoMsg(*msgPayload.mutable_msgbase());
    if (ret) {
        ret = -EINVAL;
        LogErr("Create proto register failed!");
        goto CommRet;
    }
    // msg pre send
    MsgHandler->ProtoPreSend(msgPayload);
    // send msg
    serializedData = msgPayload.SerializeAsString();
    qMsg = Util_NewSendQMsg(serializedData.size());
    if (!qMsg) {
        ret = -ENOMEM;
        LogErr("Get msg mem failed!");
        goto CommRet;
    }
    memcpy(qMsg->Cont.VarLenCont, serializedData.data(), serializedData.size());
    ret = Util_SendQMsg(WorkerFd, qMsg);
    if (ret < SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", msgPayload.ShortDebugString().c_str());
    RegisterCtx.RegisterRetried ++;

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
    uint8_t *plain = NULL;
    size_t plainLen = 0;
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
    if (worker->RegisterCtx.Status == SCMsg::SC_MSG_TYPE_REGISTER_FINISH) {
        if (worker->RegisterCtx.TransCipherSuite == SCMsg::SC_CIPHER_SUITE_SM4) {
            plainLen = recvMsg->Head.ContentLen;
            plain = new uint8_t[plainLen];
            ret = Util_CryptSm4CBCDecrypt(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen - UTIL_CRYPT_SM4_IV_LEN,
                    worker->RegisterCtx.TransKey.Sm4Key, sizeof(worker->RegisterCtx.TransKey.Sm4Key),
                    recvMsg->Cont.VarLenCont + recvMsg->Head.ContentLen - UTIL_CRYPT_SM4_IV_LEN, UTIL_CRYPT_SM4_IV_LEN,
                    plain, &plainLen);
            if (ret) {
                LogErr("Decrypt failed! ret %d", ret);
                goto CommRet;
            }
            recvMsg->Head.ContentLen = plainLen;
            memcpy(recvMsg->Cont.VarLenCont, plain, plainLen);
        }
    }

    if (!msgPayload.ParseFromArray(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen)) {
        LogErr("parse form array failed!");
        goto CommRet;
    }
    LogInfo("Recv msg: %s", msgPayload.ShortDebugString().c_str());
    ret = worker->MsgHandler->DispatchMsg(msgPayload);
    if (ret < 0) {
        LogErr("dispatch msg failed! ret %d", ret);
        goto CommRet;
    }

CommRet:
    if (recvMsg)
        Util_FreeRecvQMsg(recvMsg);
    if (plain)
        delete[] plain;
    return ;
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

void ClientWorker::_StateMachineFn(
    evutil_socket_t Fd,
    short Ev, 
    void* Arg
    )
{
    ClientWorker *worker = (ClientWorker*)Arg;
    ERR_T ret = SUCCESS;
    UNUSED(Ev);
    UNUSED(Fd);
    
    switch (worker->State) {
        case C_WORKER_STATS_INITED:
        case C_WORKER_STATS_DISCONNECTED:
            ret = worker->ConnectServer();
            if (ret < SUCCESS) {
                LogErr("Connect failed! ret %d", ret);
                goto CommRet;
            }
            ret = worker->RegisterToServer();
            if (ret < SUCCESS) {
                LogErr("Send register msg failed! ret %d", ret);
                goto CommRet;
            }
            break;
        case C_WORKER_STATS_CONNECTED:
            ret = worker->RegisterToServer();
            if (ret < SUCCESS) {
                LogErr("Send register msg failed! ret %d", ret);
                goto CommRet;
            }
            break;
        case C_WORKER_STATS_REGISTERED:
            break;
        case C_WORKER_STATS_EXIT:
        default:
            LogErr("In stat %d, exiting stat machine!", worker->State);
            Util_TimerDel(&worker->StatMachineTimer);
            worker->StatMachineTimer = NULL;
            goto CommRet;
    }
    if (worker->State != C_WORKER_STATS_REGISTERED)
        LogInfo("Worker in stat <%s>.", sg_CStatString[worker->State].c_str());

CommRet:
    return ;
}

ERR_T ClientWorker::StartStateMachine(void) {
    ERR_T ret = SUCCESS;

    ret = Util_TimerAdd(_StateMachineFn, 3 * 1000, (void*)this, UTIL_TIMER_TYPE_LOOP, TRUE, &StatMachineTimer);
    if (ret) {
        LogErr("Add timer! ret %d\n", ret);
    }

    return ret;
}

ERR_T ClientWorker::Init(C_WORKER_INIT_PARAM InitParam){
    ERR_T ret = SUCCESS;

    if (C_IS_INITED(State)) {
        goto CommRet;
    }
    
    ClientWorker::InitParam = InitParam;
    
    // workPath init
    ret = InitPath(InitParam.WorkPath);
    if (ret != SUCCESS) {
        LogErr("Init path for %d failed! ret %d", InitParam.ClientId, ret);
        goto InitErr;
    }
    // Sm2 init
    ret = InitSm2Key(InitParam.WorkPath, InitParam.PriKeyPwd);
    if (ret != SUCCESS) {
        LogErr("Init sm2 key for %d failed! ret %d", InitParam.ClientId, ret);
        goto InitErr;
    }

    MsgHandler = new ClientMsgHandler(this);
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
        if (StatMachineTimer) {
            Util_TimerDel(&StatMachineTimer);
            StatMachineTimer = NULL;
        }
        State = C_WORKER_STATS_EXIT;
    }
}

ERR_T
ClientWorker::SendMsg(uint32_t ClientId, std::string Msg) {
    SCMsg::MsgPayload sendMsgPayload;
    ERR_T ret = SUCCESS;
    std::string serializedData;
    const uint8_t* plain = NULL;
    size_t plainLen = 0;
    uint8_t *cipher = NULL;
    size_t cipherLen = 0;
    uint8_t iv[UTIL_CRYPT_SM4_IV_LEN] = {0};
    size_t ivLen = sizeof(iv);
    UTIL_Q_MSG *sendMsg = NULL;
    
    if (RegisterCtx.Status != SCMsg::SC_MSG_TYPE_REGISTER_FINISH) {
        std::cout << "Not registered, please wait or check" << '\n';
        goto CommRet;
    }

    MsgHandler->ProtoInitMsg(sendMsgPayload);
    sendMsgPayload.mutable_msgbase()->set_msgtype(SCMsg::SC_MSG_TYPE_MSG_TRANS_C_2_S);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_from()->set_clientid(InitParam.ClientId);
    // to my self
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_to()->set_clientid(ClientId);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg(Msg);
    // pre send
    MsgHandler->ProtoPreSend(sendMsgPayload);
    serializedData = sendMsgPayload.SerializeAsString();
    if (RegisterCtx.Status == SCMsg::SC_MSG_TYPE_REGISTER_FINISH &&
        RegisterCtx.TransCipherSuite == SCMsg::SC_CIPHER_SUITE_SM4) {
        cipherLen = Util_CryptSm4CBCGetPaddedLen(serializedData.size()) + sizeof(iv);
        cipher = (uint8_t*)Util_MemCalloc(MemId, cipherLen);
        if (!cipher) {
            goto CommRet;
        }
        plain = reinterpret_cast<const uint8_t*>(serializedData.c_str());
        plainLen = serializedData.size();
        ret = Util_CryptSm4CBCEncrypt(plain, plainLen, RegisterCtx.TransKey.Sm4Key, sizeof(RegisterCtx.TransKey.Sm4Key), cipher, &cipherLen, iv, &ivLen);
        if (ret || ivLen != sizeof(iv)) {
            LogErr("encrypt msg failed!");
            goto CommRet;
        }
        memcpy(cipher + cipherLen, iv, ivLen);
        cipherLen += ivLen;
        sendMsg = Util_NewSendQMsg(cipherLen);
        if (!sendMsg) {
            goto CommRet;
        }
        memcpy(sendMsg->Cont.VarLenCont, cipher, cipherLen);
    } else {
        sendMsg = Util_NewSendQMsg(serializedData.size());
        if (!sendMsg) {
            goto CommRet;
        }
        memcpy(sendMsg->Cont.VarLenCont, serializedData.data(), serializedData.size());
    }
    ret = Util_SendQMsg(WorkerFd, sendMsg);
    if (ret < SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", sendMsgPayload.ShortDebugString().c_str());

CommRet:
    if (cipher)
        free(cipher);
    if (sendMsg) 
        Util_FreeSendQMsg(sendMsg);
    return ret;
}

ClientWorker::ClientWorker() :
    WorkerFd(-1), State(C_WORKER_STATS_UNSPEC), StatMachineTimer(NULL), EventBase(NULL), RecvEvent(NULL), MsgHandler(NULL), CurrentServerPos(-1){}

ClientWorker::~ClientWorker() {}
