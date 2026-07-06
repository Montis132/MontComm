#include <chrono>

#include "ServerMsgHandler.h"
#include "ServerWorker.h"

using namespace SCMsg;

#undef LogInfo
#undef LogDbg
#undef LogWarn
#undef LogErr
#define LogInfo(FMT, ...)       LogClassInfo("SMsgHdlr", FMT, ##__VA_ARGS__)
#define LogDbg(FMT, ...)        LogClassInfo("SMsgHdlr", FMT, ##__VA_ARGS__)
#define LogWarn(FMT, ...)       LogClassInfo("SMsgHdlr", FMT, ##__VA_ARGS__)
#define LogErr(FMT, ...)        LogClassInfo("SMsgHdlr", FMT, ##__VA_ARGS__)

typedef struct _S_TPOOL_MSG_SEND_ARG{
    SCMsg::MsgPayload *SendMsg;
    int32_t Fd;
    class ServerWorker* Worker;
    ServerMsgHandler* MsgHandler;
}
S_TPOOL_MSG_SEND_ARG;

extern "C" void Server_TPoolSendMsgFunc(void* Arg) {
    S_TPOOL_MSG_SEND_ARG *arg = (S_TPOOL_MSG_SEND_ARG*)Arg;
    UTIL_Q_MSG *sendMsg = NULL;
    ERR_T ret = SUCCESS;
    uint8_t *cipher = NULL;
    size_t cipherLen = 0;
    const uint8_t* plain = NULL;
    size_t plainLen = 0;
    uint8_t iv[UTIL_CRYPT_SM4_IV_LEN] = {0};
    size_t ivLen = sizeof(iv);
    uint8_t msgpackBuf[4096];
    size_t msgpackLen = 0;
    auto *worker = arg ? arg->Worker : NULL;

    if (!arg || !arg->MsgHandler || !arg->SendMsg || !arg->Worker) {
        goto CommRet;
    }

    arg->MsgHandler->ProtoPreSend(*arg->SendMsg);
    msgpackLen = MsgPayloadEncodeToBuf(*arg->SendMsg, msgpackBuf, sizeof(msgpackBuf));
    {
    auto clientIt = worker->ClientMap.find(arg->Fd);
    if (clientIt == worker->ClientMap.end()) {
        LogErr("Client %d not found!", arg->Fd);
        goto CommRet;
    }
    auto *regCtx = &clientIt->second->RegisterCtx;

    if (regCtx->Status == SC_MSG_TYPE_REGISTER_FINISH && 
        regCtx->TransCipherSuite == SC_CIPHER_SUITE_SM4 &&
        arg->SendMsg->msgBase.msgType != SC_MSG_TYPE_REGISTER_FINISH) {
        cipherLen = Util_CryptSm4CBCGetPaddedLen(msgpackLen) + sizeof(iv);
        cipher = (uint8_t*)worker->Calloc(cipherLen);
        if (!cipher) {
            goto CommRet;
        }
        plain = msgpackBuf;
        plainLen = msgpackLen;
        Util_Hexdump("Plain-Data", (uint8_t*)plain, plainLen);
        ret = Util_CryptSm4CBCEncrypt(plain, plainLen, 
            regCtx->TransKey.Sm4Key, sizeof(regCtx->TransKey.Sm4Key), 
            cipher, &cipherLen, iv, &ivLen);
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
        Util_Hexdump("SM4-Encrypt-Data", cipher, cipherLen);
    } else {
        sendMsg = Util_NewSendQMsg(msgpackLen);
        if (!sendMsg) {
            goto CommRet;
        }
        memcpy(sendMsg->Cont.VarLenCont, msgpackBuf, msgpackLen);
    } 
    ret = Util_SendQMsg(arg->Fd, sendMsg);
    if (ret < SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", MsgPayloadToString(*arg->SendMsg).c_str());
    }

CommRet:
    if (sendMsg) 
        Util_FreeSendQMsg(sendMsg);
    if (cipher && worker)
        worker->Free(cipher);
    if (arg && arg->SendMsg) {
        arg->MsgHandler->ProtoRelease(*arg->SendMsg);
        delete arg->SendMsg;
    }
    if (arg && worker)
        worker->Free(arg);
    return ;
}

ERR_T ServerMsgHandler::SendMsgAsync(const MsgPayload MsgPayload, int32_t Fd) {
    S_TPOOL_MSG_SEND_ARG *tpoolArg = NULL;
    ERR_T ret = SUCCESS;
    
    tpoolArg = (S_TPOOL_MSG_SEND_ARG*)Worker->Calloc(sizeof(S_TPOOL_MSG_SEND_ARG));
    if (!tpoolArg) {
        return -ENOMEM;
    }
    tpoolArg->SendMsg = new SCMsg::MsgPayload(MsgPayload);
    tpoolArg->Fd = Fd;
    tpoolArg->MsgHandler = this;
    tpoolArg->Worker = Worker;
    ret = Util_TPoolAddTask(Server_TPoolSendMsgFunc, (void*)tpoolArg);
    if (ret < SUCCESS) {
        ProtoRelease(*tpoolArg->SendMsg);
        delete tpoolArg->SendMsg;
        Worker->Free(tpoolArg);
    }
    return ret;
}

void ServerMsgHandler::ProtoInitMsg(MsgPayload &MsgPayload) {
    MsgPayload.transId = TransId.load();
    MsgPayload.serverInfo.serverName = Worker->InitParam.Name;
}

void ServerMsgHandler::ProtoPreSend(MsgPayload &MsgPayload) {
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = nowMs.time_since_epoch();
    long timestamp = value.count();

    MsgPayload.timestamp = timestamp;
    TransId.fetch_add(1);
}

void ServerMsgHandler::ProtoRelease(MsgPayload &MsgPayload) {
    (void)MsgPayload;
}

ERR_T ServerMsgHandler::RegisterClient(
    int32_t Fd,
    const MsgPayload MsgPayload
    )
{
    SCMsg::MsgPayload sendMsgPayload;
    ERR_T ret = SUCCESS;
    uint8_t randTmpBuff[128];
    size_t randTmpBuffLen = sizeof(randTmpBuff);
    uint8_t plainBuff[128];
    size_t plainBuffLen = sizeof(plainBuff);
    auto *worker = Worker;

    pthread_spin_lock(&worker->Lock);
    auto it = worker->ClientMap.find(Fd);
    if (it == worker->ClientMap.end()) {
        pthread_spin_unlock(&worker->Lock);
        return -EEXIST;
    }
    S_CLIENT_NODE *clientNode = it->second;
    pthread_spin_unlock(&worker->Lock);

    ProtoInitMsg(sendMsgPayload);

    if (MsgPayload.msgBase.msgType != (uint32_t)clientNode->RegisterCtx.Status + 1 && 
        MsgPayload.msgBase.msgType != (uint32_t)clientNode->RegisterCtx.Status - 1) {
        LogDbg("Ignore invalid type %u, current %d", MsgPayload.msgBase.msgType, clientNode->RegisterCtx.Status);
        return -EEXIST;
    }

    switch (MsgPayload.msgBase.msgType) {
        case SC_MSG_TYPE_REGISTER_REQUEST:
            clientNode->ClientId = MsgPayload.msgBase.registerRequest.clientId;
            clientNode->RegisterCtx.TransCipherSuite = MsgPayload.msgBase.registerRequest.cipherSuite;
            ret = worker->MngrClient->GetPubKeyAfterGotClientId(clientNode->ClientId, clientNode->RegisterCtx.PubKey);
            if (ret) {
                LogErr("Get pubKey failed! ret %d", ret);
                goto CommRet;
            }
            ret = Util_CryptRandBytes(clientNode->RegisterCtx.ChallengeRandSend, sizeof(clientNode->RegisterCtx.ChallengeRandSend));
            if (ret) {
                LogErr("Gen Rand Bytes failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE Plain", clientNode->RegisterCtx.ChallengeRandSend, sizeof(clientNode->RegisterCtx.ChallengeRandSend));
            ret = Util_CryptSm2Encrypt(clientNode->RegisterCtx.ChallengeRandSend, sizeof(clientNode->RegisterCtx.ChallengeRandSend),
                    &clientNode->RegisterCtx.PubKey, randTmpBuff, &randTmpBuffLen);
            if (ret) {
                LogErr("Encrypt by pubkey failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE Plain-Enc", randTmpBuff, randTmpBuffLen);
            sendMsgPayload.msgBase.msgType = SC_MSG_TYPE_REGISTER_CHALLENGE;
            sendMsgPayload.msgBase.registerChallenge.cipherRand.assign(randTmpBuff, randTmpBuff + randTmpBuffLen);
            clientNode->RegisterCtx.Status = SC_MSG_TYPE_REGISTER_CHALLENGE;
            break;
        case SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY: {
            auto &receivedPlain = MsgPayload.msgBase.registerChallengeReply.plainRand;
            if (receivedPlain.size() != sizeof(clientNode->RegisterCtx.ChallengeRandSend)) {
                LogErr("Invalid plainrand size %zu", receivedPlain.size());
                ret = -E2BIG;
                goto CommRet;
            }
            if (memcmp(receivedPlain.data(), clientNode->RegisterCtx.ChallengeRandSend, receivedPlain.size()) != 0) {
                ret = -EIO;
                LogErr("Random mismatch!");
                goto CommRet;
            }
            Util_Hexdump("CHALLENGE_REPLY Plain", (uint8_t*)receivedPlain.data(), receivedPlain.size());

            auto &receivedCipher = MsgPayload.msgBase.registerChallengeReply.cipherRand;
            if (receivedCipher.size() > sizeof(randTmpBuff)) {
                ret = -E2BIG;
                LogErr("Too big cipherrand, size %zu", receivedCipher.size());
                goto CommRet;
            }
            randTmpBuffLen = receivedCipher.size();
            memcpy(randTmpBuff, receivedCipher.data(), randTmpBuffLen);
            Util_Hexdump("REGISTER_FINISH Cipher", randTmpBuff, randTmpBuffLen);
            plainBuffLen = sizeof(plainBuff);
            ret = Util_CryptSm2Decrypt(randTmpBuff, randTmpBuffLen, worker->Sm2PriKeyPath.c_str(), worker->InitParam.Sm2PriKeyPwd.c_str(),
                    plainBuff, &plainBuffLen);
            if (ret) {
                LogErr("Decrypt by prikey failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_FINISH Cipher-Dec", plainBuff, plainBuffLen);
            sendMsgPayload.msgBase.msgType = SC_MSG_TYPE_REGISTER_FINISH;
            sendMsgPayload.msgBase.registerFinish.plainRand.assign(plainBuff, plainBuff + plainBuffLen);
            if (clientNode->RegisterCtx.TransCipherSuite == SC_CIPHER_SUITE_SM4) {
                ret = Util_CryptRandBytes(clientNode->RegisterCtx.TransKey.Sm4Key, sizeof(clientNode->RegisterCtx.TransKey.Sm4Key));
                if (ret) {
                    LogErr("Gen Rand Bytes failed! ret %d", ret);
                    goto CommRet;
                }
                randTmpBuffLen = sizeof(randTmpBuff);
                Util_Hexdump("REGISTER_FINISH SM4-Key", clientNode->RegisterCtx.TransKey.Sm4Key, sizeof(clientNode->RegisterCtx.TransKey.Sm4Key));
                ret = Util_CryptSm2Encrypt(clientNode->RegisterCtx.TransKey.Sm4Key, sizeof(clientNode->RegisterCtx.TransKey.Sm4Key),
                        &clientNode->RegisterCtx.PubKey, randTmpBuff, &randTmpBuffLen);
                if (ret) {
                    LogErr("Encrypt failed! ret %d", ret);
                    goto CommRet;
                }
                Util_Hexdump("REGISTER_FINISH SM4-Key-Enc", randTmpBuff, randTmpBuffLen);
                sendMsgPayload.msgBase.registerFinish.cipherContent.cipherSM4Key.assign(randTmpBuff, randTmpBuff + randTmpBuffLen);
            } else {
                ret = -ENOSYS;
                LogErr("Unsupported crypt suite %d", clientNode->RegisterCtx.TransCipherSuite);
                goto CommRet;
            }
            sendMsgPayload.msgBase.registerFinish.cipherContent.cipherSuite = clientNode->RegisterCtx.TransCipherSuite;
            clientNode->RegisterCtx.Status = SC_MSG_TYPE_REGISTER_FINISH;
            worker->ClientCurrentNum.fetch_add(1);
            if (worker->ClientFdMap.find(clientNode->ClientId) == worker->ClientFdMap.end()) {
                worker->ClientFdMap[clientNode->ClientId] = Fd;
                LogInfo("Insert id-fd:%u %d", clientNode->ClientId, Fd);
            }
            break;
        }
        default:
            LogErr("Ignore invalid type %u", MsgPayload.msgBase.msgType);
            ret = -EEXIST;
            goto CommRet;
    }
    ret = SendMsgAsync(sendMsgPayload, Fd);
    if (ret) {
        LogErr("Send msg async failed! ret %d", ret);
    }

CommRet:
    if (ret) {
        clientNode->RegisterCtx.Status = 0;
    }
    return ret;
}

ERR_T ServerMsgHandler::TransmitMsg(
    int32_t Fd,
    const MsgPayload RecvMsgPayload
    ) 
{
    int32_t peerFd = -1;
    SCMsg::MsgPayload sendMsgPayload;
    ERR_T ret = SUCCESS;
    auto *worker = Worker;
    auto &transMsg = RecvMsgPayload.msgBase.transMsg;
    uint32_t toClientId = transMsg.to.clientId;
    uint32_t fromClientId = transMsg.from.clientId;
    decltype(worker->ClientMap)::iterator srcIt, peerNodeIt;
    decltype(worker->ClientFdMap)::iterator peerIt;
    
    peerIt = worker->ClientFdMap.find(toClientId);
    if (peerIt == worker->ClientFdMap.end()) {
        sendMsgPayload.msgBase.transMsg.msg = "Peer not registered!";
        ret = -EINVAL;
        LogErr("Peer not registered, clientid %u", toClientId);
        goto CommErr;
    }

    peerFd = peerIt->second;
    srcIt = worker->ClientMap.find(Fd);
    if (srcIt == worker->ClientMap.end() ||
        srcIt->second->RegisterCtx.Status != SCMsg::SC_MSG_TYPE_REGISTER_FINISH) {
        sendMsgPayload.msgBase.transMsg.msg = "Not registered, please wait or check!";
        ret = -EINVAL;
        LogErr("Sender not registered!");
        goto CommErr;
    }
    peerNodeIt = worker->ClientMap.find(peerFd);
    if (peerNodeIt == worker->ClientMap.end() ||
        peerNodeIt->second->RegisterCtx.Status != SCMsg::SC_MSG_TYPE_REGISTER_FINISH) {
        sendMsgPayload.msgBase.transMsg.msg = "Peer not registered, please wait or check!";
        ret = -EINVAL;
        LogErr("Peer not registered!");
        goto CommErr;
    }

    sendMsgPayload.msgBase.msgType = SCMsg::SC_MSG_TYPE_MSG_TRANS_S_2_C;
    sendMsgPayload.msgBase.transMsg.from.clientId = toClientId;
    sendMsgPayload.msgBase.transMsg.to.clientId = fromClientId;
    sendMsgPayload.msgBase.transMsg.msg = transMsg.msg;
    goto CommRet;

CommErr:
    peerFd = Fd;
    sendMsgPayload.errCode = ECONNREFUSED;
    sendMsgPayload.msgBase.msgType = SCMsg::SC_MSG_TYPE_MSG_TRANS_S_2_C;
    sendMsgPayload.msgBase.transMsg.from.clientId = fromClientId;
    sendMsgPayload.msgBase.transMsg.to.clientId = fromClientId;

CommRet:
    if (SendMsgAsync(sendMsgPayload, peerFd)) {
        LogErr("Send msg async failed!");
    }
    return ret;
}

ERR_T ServerMsgHandler::DispatchMsg(
    int32_t Fd,
    const MsgPayload MsgPayload
    ) 
{
    ERR_T ret = SUCCESS;
    switch (MsgPayload.msgBase.msgType) {
        case SC_MSG_TYPE_REGISTER_REQUEST:
        case SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY:
            if (Worker->ClientCurrentNum.load() < Worker->InitParam.Load) {
                ret = RegisterClient(Fd, MsgPayload);
            } else {
                LogWarn("ClientNum has reached its upper limit %u, ignore connect request!", Worker->ClientCurrentNum.load());
                ret = -EBUSY;
            }
            break;
        case SC_MSG_TYPE_MSG_TRANS_C_2_S:
            ret = TransmitMsg(Fd, MsgPayload);
            break;
        default:
            LogErr("Invalid type:%d", MsgPayload.msgBase.msgType);
            return -EIO;
    }
    return ret;
}

ServerMsgHandler::ServerMsgHandler(class ServerWorker* InWorker):TransId(0){
    TransId.fetch_add(1);
    Worker = InWorker;
};
ServerMsgHandler::~ServerMsgHandler(){};
