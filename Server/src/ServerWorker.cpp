#include <filesystem>

#include "SCMsg.pb.h"
#include "ServerWorker.h"

using namespace std;

#undef LogInfo
#undef LogDbg
#undef LogWarn
#undef LogErr
#define LogInfo(FMT, ...)       LogClassInfo("SvrWrkr", FMT, ##__VA_ARGS__)
#define LogDbg(FMT, ...)        LogClassInfo("SvrWrkr", FMT, ##__VA_ARGS__)
#define LogWarn(FMT, ...)       LogClassInfo("SvrWrkr", FMT, ##__VA_ARGS__)
#define LogErr(FMT, ...)        LogClassInfo("SvrWrkr", FMT, ##__VA_ARGS__)

#define SERVER_WORKER_PUBKEY_FILE                            "S_SM2_Pub.pem"
#define SERVER_WORKER_PRIKEY_FILE                            "S_SM2_Pri.pem"

ERR_T 
ServerWorker::InitPath(std::string WorkPath) {
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
ServerWorker::InitSm2Key(std::string CryptoPath, std::string PriKeyPwd) {
    ERR_T ret = SUCCESS;
    std::filesystem::path cryptoDir = CryptoPath;
    std::filesystem::path pubkeyFile = cryptoDir / SERVER_WORKER_PUBKEY_FILE;
    std::filesystem::path prikeyFile = cryptoDir / SERVER_WORKER_PRIKEY_FILE;
    
    try {
        if (!std::filesystem::exists(pubkeyFile) || !std::filesystem::exists(prikeyFile)) {
            ret = Util_CryptSm2KeyGenAndExport(pubkeyFile.string().c_str(), prikeyFile.string().c_str(), PriKeyPwd.c_str());
            if (ret) {
                LogErr("GenKey failed! ret %d", ret);
                return ret;
            } else {
                LogDbg("Genkey success!");
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LogErr("Failed to check file existence. %s", e.what());
        return -EIO;
    }
    ServerWorker::Sm2PriKeyPath = prikeyFile.string();

    ret = Util_CryptSm2ImportPubKey(pubkeyFile.string().c_str(), &Sm2PubKey);
    if (ret) {
        LogErr("Import key failed! ret %d", ret);
        return ret;
    }
    Util_Hexdump("Local-Sm2PubKey", (uint8_t*)&Sm2PubKey.public_key, sizeof(Sm2PubKey.public_key));

    return ret;
}

ERR_T
ServerWorker::InitWorkerFd(uint16_t Port, uint32_t Load) {
    struct sockaddr_in serverAddr;
    ERR_T ret = SUCCESS;
    int option = 1, flags = 0;
    // socket
    WorkerFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (WorkerFd < 0) {
        LogErr("Socket failed!");
        ret = -EBADFD; 
        goto CreateFdFail;
    }
    // reuse && nonblock
    if (setsockopt(WorkerFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1) {
        LogErr("setsockopt SO_REUSEADDR failed!");
        ret = -EIO; 
        goto CommErr;
    }
    flags = fcntl(WorkerFd, F_GETFL, 0);
    if (flags == -1) {
        LogErr("get flags failed!");
        ret = -EIO; 
        goto CommErr;
    }
    flags |= O_NONBLOCK;
    if (fcntl(WorkerFd, F_SETFL, flags) == -1) {
        LogErr("set nonblock failed!");
        ret = -EIO; 
        goto CommErr;
    }
    // bind port
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(Port);
    if (bind(WorkerFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        ret = -EBADF; 
        goto CommErr;
    }
    // listen
    if (listen(WorkerFd, Load) == -1) {
        LogErr("listen failed!");
        ret = -EIO; 
        goto CommErr;
    }

    LogInfo("Create server worker, fd %d, loading %u.", WorkerFd, Load);
    goto Success;
    
CommErr:
    close(WorkerFd);
    WorkerFd = -1;
CreateFdFail:
Success:
    return ret;
}

void ServerWorker::EraseClient_NL(int32_t Fd) {
    auto it = ClientMap.find(Fd);
    if (it != ClientMap.end()) {
        S_CLIENT_NODE* clientNode = it->second;
        if (clientNode) {
            uint32_t clientId = clientNode->ClientId;
            LogWarn("[%s] Erasing client %u, Fd %d.", InitParam.Name.c_str(), clientId, Fd);
            if (clientNode->RecvEvent) {
                epoll_ctl(EpollFd, EPOLL_CTL_DEL, Fd, NULL);
                Free(clientNode->RecvEvent);
            }
            if (clientNode->RegisterCtx.Status == SCMsg::SC_MSG_TYPE_REGISTER_FINISH && ClientCurrentNum.load() > 0) {
                ClientCurrentNum.fetch_sub(1);
            }
            if (clientNode->Fd >= 0) {
                close(clientNode->Fd);
            }
            Free(clientNode);
            auto iter = ClientFdMap.find(clientId);
            if (iter != ClientFdMap.end()) {
                ClientFdMap.erase(iter);
            }
        }
        ClientMap.erase(it);
    }
}

void ServerWorker::EraseClient(int32_t Fd) {
    pthread_spin_lock(&Lock);
    EraseClient_NL(Fd);
    pthread_spin_unlock(&Lock);
}

void ServerWorker::ClientRecv(int32_t Fd) {
    ERR_T ret = SUCCESS;
    UTIL_Q_MSG *recvMsg = NULL;
    SCMsg::MsgPayload msgPayload;
    uint8_t *msgPlain = NULL;
    size_t msgPlainLen = 0;

    recvMsg = Util_NewRecvQMsg();
    if (!recvMsg) {
        LogErr("Apply failed!");
        return ;
    }
    ret = Util_RecvQMsg(Fd, recvMsg);
    if (ret < 0) {
        EraseClient(Fd);
        LogErr("recv failed!");
        goto CommRet;
    }
    {
    auto it = ClientMap.find(Fd);
    if (it == ClientMap.end()) {
        LogErr("client not found!");
        goto CommRet;
    }
    auto *regCtx = &it->second->RegisterCtx;
    if (regCtx->Status != SCMsg::SC_MSG_TYPE_REGISTER_FINISH) {
        if (!msgPayload.ParseFromArray(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen)) {
            LogErr("parse form array failed!");
            goto CommRet;
        }
    } else if (regCtx->TransCipherSuite == SCMsg::SC_CIPHER_SUITE_SM4){
        msgPlainLen = recvMsg->Head.ContentLen;
        msgPlain = (uint8_t*)Calloc(msgPlainLen);
        if (!msgPlain) {
            goto CommRet;
        }
        ret = Util_CryptSm4CBCDecrypt(recvMsg->Cont.VarLenCont, recvMsg->Head.ContentLen - UTIL_CRYPT_SM4_IV_LEN, 
            regCtx->TransKey.Sm4Key, sizeof(regCtx->TransKey.Sm4Key), 
            recvMsg->Cont.VarLenCont + recvMsg->Head.ContentLen - UTIL_CRYPT_SM4_IV_LEN, UTIL_CRYPT_SM4_IV_LEN, msgPlain, &msgPlainLen);
        if (ret) {
            LogErr("decrypt msg failed!");
            goto CommRet;
        }
        if (!msgPayload.ParseFromArray(msgPlain, msgPlainLen)) {
            LogErr("parse form array failed!");
            goto CommRet;
        }
    } else {
        ret = -EINVAL;
        LogWarn("Ignore msg!");
        goto CommRet;
    }
    }
    LogInfo("[%s]recv msg: %s", InitParam.Name.c_str(), msgPayload.ShortDebugString().c_str());
    ret = MsgHandler->DispatchMsg(Fd, msgPayload);
    if (ret < 0) {
        LogErr("dispatch msg failed! ret %d", ret);
    }

CommRet:
    if (recvMsg)
        Util_FreeRecvQMsg(recvMsg);
    if (msgPlain) 
        Free(msgPlain);
}

void
ServerWorker::ServerAccept(void) {
    struct sockaddr_in clientAddr;
    socklen_t len = 0;
    int clientFd = -1;
    S_CLIENT_NODE *clientNode = NULL;
    struct timeval tv;
    ERR_T ret = SUCCESS;

    clientFd = accept(WorkerFd, (struct sockaddr*)&clientAddr, &len);
    if (clientFd < 0) {
        ret = -EIO;
        LogErr("accept failed! %d:%s", errno, StrErr(errno));
        goto CommRet;
    }
    LogInfo("[%s] %s connect.", InitParam.Name.c_str(), inet_ntoa(clientAddr.sin_addr));
    // timeout
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        ret = -EIO;
        LogErr("Set recv timeout failed\n");
        goto CommRet;
    }
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        ret = -EIO;
        LogErr("Set send timeout failed\n");
        goto CommRet;
    }
    clientNode = (S_CLIENT_NODE*)Calloc(sizeof(S_CLIENT_NODE));
    if (!clientNode) {
        ret = -ENOMEM;
        LogErr("new client node failed!");;
        goto CommRet;
    }
    clientNode->RecvEvent = (struct epoll_event*)Calloc(sizeof(struct epoll_event));
    if (!clientNode->RecvEvent) {
        ret = -ENOMEM;
        LogErr("new event failed!");
        goto CommRet;
    }
    clientNode->Fd = clientFd;
    clientNode->RecvEvent->events = EPOLLIN;        //we do not use EPOLLET here
    clientNode->RecvEvent->data.fd = clientFd;
    clientNode->RegisterCtx.Status = 0;
    epoll_ctl(EpollFd, EPOLL_CTL_ADD, clientFd, clientNode->RecvEvent);
    
    pthread_spin_lock(&Lock);
    ClientMap.insert(make_pair(clientFd, clientNode));
    pthread_spin_unlock(&Lock);

CommRet:
    if (ret < SUCCESS) {
        if (clientFd >= 0)
            close (clientFd);
        if (clientNode && !clientNode->RecvEvent) {
            Free(clientNode);
        }
    }
    return ;
}

#define S_WAIT_MAX_EVENTS                             256
#define S_WAIT_INTERVAL                               100 // ms

void*
ServerWorker::_WorkerThreadFn(void *Arg) {
    ServerWorker *worker = (ServerWorker*)Arg;
    struct epoll_event events[S_WAIT_MAX_EVENTS];
    int nfds = 0;
    
    while (worker->Inited) {
        nfds = epoll_wait(worker->EpollFd, events, S_WAIT_MAX_EVENTS, S_WAIT_INTERVAL);
        if (nfds < 0) {
            LogErr("epoll_wait err! %d:%s", errno, strerror(errno));
            continue;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == worker->WorkerFd) {
                worker->ServerAccept();
            } else if (events[i].events & EPOLLIN) {
                worker->ClientRecv(events[i].data.fd);
            } else if ( events[i].events & EPOLLERR || 
                        events[i].events & EPOLLRDHUP ||
                        events[i].events & EPOLLHUP) {
                worker->EraseClient(events[i].data.fd);
            }
        }
    }

    return NULL;
}

ERR_T ServerWorker::InitEventBaseAndRun() {
    ERR_T ret = SUCCESS;
    pthread_attr_t attr;
    BOOL initPthreadAttr = FALSE;
    int result = 0;

    EpollFd = epoll_create1(0);
    if (EpollFd < 0) {
        LogErr("create epoll fd failed!\n");
        goto NewEventBaseErr;
    }
    ListenEvent.events = EPOLLIN | EPOLLET;
    ListenEvent.data.fd = WorkerFd;
    epoll_ctl(EpollFd, EPOLL_CTL_ADD, WorkerFd, &ListenEvent);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    initPthreadAttr = TRUE;

    result = pthread_create(&ThreadId, &attr, _WorkerThreadFn, (void*)this);
    if (result != 0) {
        ret = -ENOMEM;
        LogErr("create thread failed! ret %d errno %d\n", result, errno);
        goto CommRet;
    }

    goto CommRet;

NewEventBaseErr:
    EpollFd = -1;
CommRet:
    if (initPthreadAttr)
        pthread_attr_destroy(&attr);
    return ret;
}

ServerWorker::ServerWorker() {
    WorkerFd = -1;
    Inited = FALSE;
    MemId = UTIL_MEM_MODULE_INVALID_ID;
    EpollFd = -1;
    MsgHandler = NULL;
    ClientCurrentNum.store(0);
}

ServerWorker::~ServerWorker() {
    if (WorkerFd != -1) {
        close(WorkerFd);
    }
}

void* ServerWorker::Calloc(size_t Size) {
    return Util_MemCalloc(MemId, Size);
}

void ServerWorker::Free(void* Ptr) {
    Util_MemFree(MemId, Ptr);
}

ERR_T ServerWorker::InitClientMap(void) {
    pthread_spin_init(&Lock, PTHREAD_PROCESS_PRIVATE);

    return SUCCESS;
}

ERR_T ServerWorker::Init(S_WORKER_INIT_PARAM InInitParam) {
    ERR_T ret = SUCCESS;
    if (Inited) {
        goto CommonReturn;
    }
    Inited = TRUE;
    InitParam = InInitParam;
    MsgHandler = new ServerMsgHandler(this);
    if (!MsgHandler) {
        LogErr("New msghandler for %s failed!", InitParam.Name.c_str());
        goto InitErr;
    }
    // workPath init
    ret = InitPath(InitParam.WorkPath);
    if (ret != SUCCESS) {
        LogErr("Init path for %s failed! ret %d", InitParam.Name.c_str(), ret);
        goto InitErr;
    }
    // Sm2 init
    ret = InitSm2Key(InitParam.WorkPath, InitParam.Sm2PriKeyPwd);
    if (ret != SUCCESS) {
        LogErr("Init sm2 key for %s failed! ret %d", InitParam.Name.c_str(), ret);
        goto InitErr;
    }
    // mem init
    ret = Util_MemRegister(&MemId, (char*)InitParam.Name.c_str());
    if (ret != SUCCESS) {
        LogErr("Register mem module for %s failed!", InitParam.Name.c_str());
        goto InitErr;
    }
    // socket init
    ret = InitWorkerFd(InitParam.Port, InitParam.Load);
    if (ret != SUCCESS) {
        LogErr("Init worker for %s failed!", InitParam.Name.c_str());
        goto InitErr;
    }
    // init Client Map 
    ret = InitClientMap();
    if (ret != SUCCESS) {
        LogErr("Init client map for %s failed! ret %d", InitParam.Name.c_str(), ret);
        goto InitErr;
    }
    // init EventBase And start thread
    ret = InitEventBaseAndRun();
    if (ret != SUCCESS) {
        LogErr("Init event base for %s failed! ret %d", InitParam.Name.c_str(), ret);
        goto InitErr;
    }
    
    goto CommonReturn;

InitErr:
    Exit();
CommonReturn:
    return ret;
}

void ServerWorker::Exit() {
    if (Inited) {
        if (WorkerFd != -1) {
            close(WorkerFd);
            WorkerFd = -1;
        }
        if (EpollFd >= 0) {
            close(EpollFd);
            EpollFd = -1;
        }
        if (MsgHandler) {
            delete MsgHandler;
            MsgHandler = NULL;
        }
        pthread_spin_lock(&Lock);
        for (auto it = ClientMap.begin(); it != ClientMap.end(); ) {
            S_CLIENT_NODE* clientNode = it->second;
            if (clientNode) {
                LogWarn("[%s] Erasing client %u, Fd %d.", 
                    InitParam.Name.c_str(), clientNode->ClientId, clientNode->Fd);
                if (clientNode->RecvEvent) {
                    Free(clientNode->RecvEvent);
                    clientNode->RecvEvent = NULL;
                }
                clientNode->RegisterCtx.Status = 0;
                if (clientNode->Fd >= 0) {
                    close(clientNode->Fd);
                }
                Free(clientNode);
            }
            it = ClientMap.erase(it);
        }
        for (auto it = ClientFdMap.begin(); it != ClientFdMap.end(); ) {
            it = ClientFdMap.erase(it);
        }
        pthread_spin_unlock(&Lock);
        pthread_spin_destroy(&Lock);
        Util_MemUnRegister(&MemId);
        LogDbg("%s exited.", InitParam.Name.c_str());
        Inited = FALSE;
    }
    return ;
}

string ServerWorker::GetStatus(void) {
    string statsBuff;
    int cnt = 0;

    statsBuff += "ServerWorker:" + InitParam.Name + " CurrentClientNum:" + to_string(ClientCurrentNum.load())+ '\n';
    pthread_spin_lock(&Lock);
    for (auto it = ClientMap.begin(); it != ClientMap.end(); it ++) {
        statsBuff += "[Fd:" + to_string(it->second->Fd) + " " + "ClientId:" + to_string(it->second->ClientId) + " ";
        if (++cnt % 4 == 0)
            statsBuff += "\n";
    }
    pthread_spin_unlock(&Lock);
    statsBuff += "\n";
    
    return statsBuff;
}