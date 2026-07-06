#include <iostream>
#include <fstream>
#include <string>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <nlohmann/json.hpp>

#include "UtilsModuleCommon.h"
#include "UtilsCommonUtil.h"
#include "MSMsg.h"
#include "CommMngrClient.h"
#include "SCShare.h"

#undef LogInfo
#undef LogDbg
#undef LogWarn
#undef LogErr
#define LogInfo(FMT, ...)       LogClassInfo("CommMngrC", FMT, ##__VA_ARGS__)
#define LogDbg(FMT, ...)        LogClassInfo("CommMngrC", FMT, ##__VA_ARGS__)
#define LogWarn(FMT, ...)       LogClassInfo("CommMngrC", FMT, ##__VA_ARGS__)
#define LogErr(FMT, ...)        LogClassInfo("CommMngrC", FMT, ##__VA_ARGS__)

ERR_T CommMngrClient::InitClientFd(void) {
    ERR_T ret = SUCCESS;
    struct timeval tv;
    int32_t reuseable = 1; // set port reuseable when fd closed
    // socket
    ClientFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientFd < 0) {
        LogErr("Socket failed!");
        ret = -EBADFD; 
        goto CommRet;
    }
    // reuse
    if (setsockopt(ClientFd, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(reuseable)) == -1) {
        LogErr("setsockopt SO_REUSEADDR failed!");
        ret = -EIO; 
        goto CommErr;
    }
    // timeout
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(ClientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        ret = -EIO; 
        LogErr("Set recv timeout failed");
        goto CommErr;
    }
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(ClientFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        ret = -EIO; 
        LogErr("Set send timeout failed");
        goto CommErr;
    }
    // set ctx
    ClientCtx = SSL_CTX_new(TLS_client_method());
    if (ClientCtx ==  NULL) {
        ret = -EIO; 
        LogErr("SSL_CTX_new error");
        goto CommErr;
    }
    SSL_CTX_set_verify(ClientCtx, SSL_VERIFY_PEER, NULL);
    //SSL_CTX_set_default_verify_paths(ClientCtx);
    if (InitParam.TrustedCert.length() != 0) {
        if (!SSL_CTX_load_verify_locations(ClientCtx, InitParam.TrustedCert.c_str(), NULL)) {
            LogErr("Unable to load trust certificate");
            ret = -EIO; 
            goto CommErr;
        }
        LogInfo("set trusted cert %s success", InitParam.TrustedCert.c_str());
    }
    //set ssl
    ClientSSL = SSL_new(ClientCtx);
    if (ClientSSL ==  NULL) {
        ret = -EIO; 
        LogErr("SSL_new error");
        goto CommErr;
    }
    goto CommRet;
    
CommErr:
    close(ClientFd);
    ClientFd = -1;
CommRet:
    return ret;
} 
void CommMngrClient::CloseClientFd(void) {
    if (ClientFd >= 0) {
        close(ClientFd);
        ClientFd = -1;
    }
    if (ClientSSL) {
        SSL_shutdown(ClientSSL);
        SSL_free(ClientSSL);
        ClientSSL = NULL;
    }
    if (ClientCtx) {
        SSL_CTX_free(ClientCtx);
        ClientCtx = NULL;
    }
}
ERR_T CommMngrClient::ConnectServer(void) {
    ERR_T ret = SUCCESS;
    struct sockaddr_in serverAddr;
    uint32_t ip = 0;
    uint16_t port = 0;
    int res = 0;

    serverAddr.sin_family = AF_INET;
    ret = Util_ParseStringToIpv4AndPort(InitParam.ServerAddr.c_str(), InitParam.ServerAddr.length(), &ip, &port);
    if (ret < SUCCESS || ip == 0 || port == 0) {
        LogErr("Parse %s failed!", InitParam.ServerAddr.c_str());
        goto CommRet;
    }
    serverAddr.sin_addr.s_addr = htonl(ip);
    serverAddr.sin_port = htons(port);
    if (connect(ClientFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        ret = -ENOTCONN;
        LogErr("Connect to %s(%u:%u) failed, errno %d:%s", InitParam.ServerAddr.c_str(), ip, port, errno, StrErr(errno));
        goto CommRet;
    }
    LogInfo("Tcp create connect with %s(%u:%u) success.", InitParam.ServerAddr.c_str(), ip, port);

    SSL_set_fd(ClientSSL, ClientFd);
    while(1) {
        res = SSL_connect(ClientSSL);
        if (res != 1) {
            ret = SSLErrorShow(ClientSSL, ret);
            if (ret == -EAGAIN) {
                LogInfo("Neet to try again");
                continue;
            } else {
                LogErr("SSL connect failed! ret %d:%s", ret, StrErr(-ret));
                goto CommRet;
            }
        } else {
            break;
        }
    }
    LogInfo("SSL create connect success.");
    AddRecvEvent();

CommRet:
    return ret;
}

void CommMngrClient::_RecvMsg(evutil_socket_t Fd, short Event, void *Arg) {
    ERR_T ret = SUCCESS;
    CommMngrClient *worker = (CommMngrClient*)Arg;
    uint8_t buffer[1024*1024] = {0};
    int buffLen = (int)sizeof(buffer);
    MSMsg::MsgPayload msgPayload;
    UNUSED(Fd);
    UNUSED(Event);

    ret = SSL_read(worker->ClientSSL, buffer, buffLen);
    if (ret <= 0)
    {
        ret = SSLErrorShow(worker->ClientSSL, ret);
        if (ret != -EAGAIN) {
            LogErr("Recv Msg failed, ret %d:%s", ret, StrErr(-ret));
            worker->RemoveRecvEvent();
        }
        goto CommRet;
    }
    if (!MSMsg::MsgPayloadDecodeFromBuf(msgPayload, (uint8_t*)buffer, ret)) {
        LogErr("parse form array failed!");
        goto CommRet;
    }
    LogInfo("Recv msg: %s", MSMsg::MsgPayloadToString(msgPayload).c_str());
    // handle msg

CommRet:
    return ;
}

void CommMngrClient::AddRecvEvent(void) {
    if (!RecvEvent && EventBase) {
        RecvEvent = event_new(EventBase, ClientFd, EV_READ|EV_PERSIST, _RecvMsg, (void*)this);
        event_add(RecvEvent, NULL);
        State = MNGR_CLIENT_STATS_CONNECTED;
        LogDbg("Adding recv event into base, set connected");
    }
}

void CommMngrClient::RemoveRecvEvent(void) {
    if (RecvEvent) {
        event_del(RecvEvent);
        event_free(RecvEvent);
        RecvEvent = NULL;
        LogDbg("Recv event deleted from base, set disconnected");
        State = MNGR_CLIENT_STATS_DISCONNECTED;
    }
}

void CommMngrClient::_Keepalive(evutil_socket_t Fd, short Event, void *Arg) {
    CommMngrClient *worker = (CommMngrClient*)Arg;
    UNUSED(Fd);
    UNUSED(Event);

    if (worker->State == MNGR_CLIENT_STATS_DISCONNECTED) {
        // try reconnect
        worker->CloseClientFd();
        worker->InitClientFd();
        worker->ConnectServer();
    }
}

void CommMngrClient::_HealthMonitor(evutil_socket_t Fd, short Event, void *Arg) {
    CommMngrClient *worker = (CommMngrClient*)Arg;
    static uint64_t oldTotalTime = 0, oldIdleTime = 0;
    uint64_t newTotalTime = 0, newIdleTime = 0;
    MSMsg::MsgPayload msgPayload;

    ERR_T ret = SUCCESS;
    std::string httpRequest;
    float memUsage = 0;

    UNUSED(Fd);
    UNUSED(Event);
    msgPayload.msgBase.msgType = MSMsg::MS_MSG_TYPE_SVR_HEALTH_REPORT;

    if (worker->State != MNGR_CLIENT_STATS_CONNECTED) {
        LogInfo("Not connected, return");
        return ;
    }
    ret = Util_GetCpuTime(&newTotalTime, &newIdleTime);
    if (ret < SUCCESS) {
        return ;
    }
    ret = Util_GetMemUsage(&memUsage);
    if (ret < SUCCESS) {
        return ;
    }
    msgPayload.msgBase.svrHealthReport.memUsage = memUsage;

    if (oldTotalTime != 0 || oldIdleTime != 0) {
        float cpuUsage = 0;
        if (newTotalTime > oldTotalTime) {
            cpuUsage = 1.0 - (float)(newIdleTime - oldIdleTime)/(float)(newTotalTime - oldTotalTime);
            cpuUsage *= 100;
            msgPayload.msgBase.svrHealthReport.cpuUsage = cpuUsage;
        } else {
            cpuUsage = 0;
            msgPayload.msgBase.svrHealthReport.cpuUsage = cpuUsage;
        }
    }
    oldTotalTime = newTotalTime;
    oldIdleTime = newIdleTime;

    uint8_t msgpackBuf[4096]; size_t msgpackLen = MSMsg::MsgPayloadEncodeToBuf(msgPayload, msgpackBuf, sizeof(msgpackBuf));
    ret = SSL_write(worker->ClientSSL, msgpackBuf, msgpackLen);
    if (ret <= 0)
    {
        ret = SSLErrorShow(worker->ClientSSL, ret);
        if (ret != -EAGAIN) {
            LogErr("Send Msg failed, ret %d:%s", ret, StrErr(-ret));
            worker->RemoveRecvEvent();
        }
        goto CommRet;
    }
    LogInfo("Send Msg: %s", MSMsg::MsgPayloadToString(msgPayload).c_str());
    
CommRet:
    // msgPayload auto-released
    return ;
}

void*
CommMngrClient::_MngrCThreadFn(void *Arg) {
    CommMngrClient *worker = (CommMngrClient*)Arg;
    
    event_base_dispatch(worker->EventBase);
    // break at here
    LogDbg("event base loop break here!");
    event_base_free(worker->EventBase);
    worker->EventBase = NULL;

    return NULL;
}

ERR_T CommMngrClient::InitEventBaseAndRun(void) {
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
    
    tv.tv_sec = 60;
    tv.tv_usec = 0;
    KeepaliveEvent = event_new(EventBase, -1, EV_READ|EV_PERSIST, _Keepalive, (void*)this);
    if (!KeepaliveEvent) {
        ret = -ENOMEM;
        LogErr("Create keepalive event base failed!");
        goto CommRet;
    }
    event_add(KeepaliveEvent, &tv);
    event_active(KeepaliveEvent, 0, EV_READ);   // Must be actively activated once, otherwise it will not run

    tv.tv_sec = 60;
    tv.tv_usec = 0;
    HealthMonitorEvent = event_new(EventBase, -1, EV_READ|EV_PERSIST, _HealthMonitor, (void*)this);
    if (!HealthMonitorEvent) {
        ret = -ENOMEM;
        LogErr("Create health monitor event base failed!");
        goto CommRet;
    }
    event_add(HealthMonitorEvent, &tv);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    initPthreadAttr = TRUE;

    result = pthread_create(&ThreadId, &attr, _MngrCThreadFn, (void*)this);
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

ERR_T CommMngrClient::Init(COMM_MNGR_CLIENT_INIT_PARAM InitParam) {
    ERR_T ret = SUCCESS;
    
    if (MNGR_C_IS_INITED(State)) {
        goto CommRet;
    }

    this->InitParam = InitParam;

    ret = InitClientFd();
    if (ret < SUCCESS) {
        LogErr("Init fd failed! ret %d", ret);
        goto InitErr;
    }
    State = MNGR_CLIENT_STATS_INITED;
    ret = InitEventBaseAndRun();
    if (ret < SUCCESS) {
        LogErr("Enter work fn failed! ret %d", ret);
        goto InitErr;
    }
    ret = ConnectServer();
    if (ret < SUCCESS) {
        LogErr("Connect server failed! try reconnect, ret %d", ret);
        ret = SUCCESS;
        State = MNGR_CLIENT_STATS_DISCONNECTED;
        goto CommRet;
    }
    LogInfo("Mngr client init success");
    goto CommRet;

InitErr:
    Exit();
CommRet:
    return ret;
}

void CommMngrClient::Exit(void) {
    if (MNGR_C_IS_INITED(State)) {
        CloseClientFd();
        if (EventBase) {
            RemoveRecvEvent();
            State = MNGR_CLIENT_STATS_EXIT;
            event_base_loopbreak(EventBase);
            if (KeepaliveEvent) {
                event_del(KeepaliveEvent);
                event_free(KeepaliveEvent);
                KeepaliveEvent = NULL;
            }
            if (HealthMonitorEvent) {
                event_del(HealthMonitorEvent);
                event_free(HealthMonitorEvent);
                HealthMonitorEvent = NULL;
            }
            if (RecvEvent) {
                event_del(RecvEvent);
                event_free(RecvEvent);
                RecvEvent = NULL;
            }
        }
    }
}

ERR_T CommMngrClient::GetPubKeyAfterGotClientId(uint32_t ClientId, SM2_KEY &PubKey){
    // todo
    std::string title = "Client-" + std::to_string(ClientId) + "-SM2PubKey";
    ERR_T ret = SUCCESS;

    ret =  Util_CryptSm2ImportPubKey("CWorkPath/C_SM2_Pub.pem", &PubKey);
    
    Util_Hexdump(title.c_str(), (uint8_t*)&PubKey.public_key, sizeof(PubKey.public_key));
    return ret;
}

CommMngrClient::CommMngrClient(): 
    ClientFd(-1), ClientSSL(NULL), ClientCtx(NULL), EventBase(NULL), 
    KeepaliveEvent(NULL), HealthMonitorEvent(NULL), RecvEvent(NULL), State(MNGR_CLIENT_STATS_UNSPEC){}
CommMngrClient::~CommMngrClient() {}