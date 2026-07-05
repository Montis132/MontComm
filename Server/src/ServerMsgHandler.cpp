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
    ServerWorker* Worker;
    ServerMsgHandler* MsgHandler;
}
S_TPOOL_MSG_SEND_ARG;

extern "C" void Server_TPoolSendMsgFunc(void* Arg) {
    S_TPOOL_MSG_SEND_ARG *arg = (S_TPOOL_MSG_SEND_ARG*)Arg;
    UTIL_Q_MSG *sendMsg = NULL;
    std::string serializedData;
    ERR_T ret = SUCCESS;
    uint8_t *cipher = NULL;
    size_t cipherLen = 0;
    const uint8_t* plain = NULL;
    size_t plainLen = 0;
    uint8_t iv[UTIL_CRYPT_SM4_IV_LEN] = {0};
    size_t ivLen = sizeof(iv);

    if (!arg || !arg->MsgHandler || !arg->SendMsg || !arg->Worker) {
        goto CommRet;
    }
    arg->MsgHandler->ProtoPreSend(*arg->SendMsg);
    serializedData = arg->SendMsg->SerializeAsString();

    auto *worker = arg->Worker;
    auto clientIt = worker->ClientMap.find(arg->Fd);
    if (clientIt == worker->ClientMap.end()) {
        LogErr("Client %d not found!", arg->Fd);
        goto CommRet;
    }
    auto *regCtx = &clientIt->second->RegisterCtx;

    if (regCtx->Status == SC_MSG_TYPE_REGISTER_FINISH && 
        regCtx->TransCipherSuite == SC_CIPHER_SUITE_SM4 &&
        arg->SendMsg->msgbase().msgtype() != SC_MSG_TYPE_REGISTER_FINISH) {
        cipherLen = Util_CryptSm4CBCGetPaddedLen(serializedData.size()) + sizeof(iv);
        cipher = (uint8_t*)worker->Calloc(cipherLen);
        if (!cipher) {
            goto CommRet;
        }
        plain = reinterpret_cast<const uint8_t*>(serializedData.c_str());
        plainLen = serializedData.size();
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
        sendMsg = Util_NewSendQMsg(serializedData.size());
        if (!sendMsg) {
            goto CommRet;
        }
        memcpy(sendMsg->Cont.VarLenCont, serializedData.data(), serializedData.size());
    } 
    ret = Util_SendQMsg(arg->Fd, sendMsg);
    if (ret < SUCCESS) {
        LogErr("send msg failed! ret %d", ret);
        goto CommRet;
    }
    LogInfo("Send Msg: %s", arg->SendMsg->ShortDebugString().c_str());

CommRet:
    if (sendMsg) 
        Util_FreeSendQMsg(sendMsg);
    if (cipher)
        worker->Free(cipher);
    if (arg->SendMsg) {
        arg->MsgHandler->ProtoRelease(*arg->SendMsg);
        delete arg->SendMsg;
    }
    worker->Free(arg);
    return ;
}

ERR_T ServerMsgHandler::SendMsgAsync(const MsgPayload MsgPayload, int32_t Fd) {
    S_TPOOL_MSG_SEND_ARG *tpoolArg = NULL;
    ERR_T ret = SUCCESS;
    
    tpoolArg = (S_TPOOL_MSG_SEND_ARG*)ServerWorker->Calloc(sizeof(S_TPOOL_MSG_SEND_ARG));
    if (!tpoolArg) {
        return -ENOMEM;
    }
    tpoolArg->SendMsg = new SCMsg::MsgPayload(MsgPayload);
    tpoolArg->Fd = Fd;
    tpoolArg->MsgHandler = this;
    tpoolArg->Worker = ServerWorker;
    ret = Util_TPoolAddTask(Server_TPoolSendMsgFunc, (void*)tpoolArg);
    if (ret < SUCCESS) {
        ProtoRelease(*tpoolArg->SendMsg);
        delete tpoolArg->SendMsg;
        ServerWorker->Free(tpoolArg);
    }
    return ret;
}

void ServerMsgHandler::ProtoInitMsg(
    MsgPayload &MsgPayload
    )
{
    UNUSED(MsgPayload.mutable_msgbase());
    UNUSED(MsgPayload.mutable_serverinfo());
    MsgPayload.set_transid(TransId.load());
    MsgPayload.mutable_serverinfo()->set_servername(ServerWorker->InitParam.Name);
}

void ServerMsgHandler::ProtoPreSend(MsgPayload &MsgPayload) {
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = nowMs.time_since_epoch();
    long timestamp = value.count();

    MsgPayload.set_timestamp(timestamp);
    TransId.fetch_add(1);
}

void ServerMsgHandler::ProtoRelease(MsgPayload &MsgPayload) {
    UNUSED(MsgPayload.release_msgbase());
    UNUSED(MsgPayload.release_serverinfo());
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
    auto *worker = ServerWorker;

    pthread_spin_lock(&worker->Lock);
    auto it = worker->ClientMap.find(Fd);
    if (it == worker->ClientMap.end()) {
        pthread_spin_unlock(&worker->Lock);
        return -EEXIST;
    }
    S_CLIENT_NODE *clientNode = it->second;
    pthread_spin_unlock(&worker->Lock);

    ProtoInitMsg(sendMsgPayload);

    if (MsgPayload.msgbase().msgtype() != (uint32_t)clientNode->RegisterCtx.Status + 1 && 
        MsgPayload.msgbase().msgtype() != (uint32_t)clientNode->RegisterCtx.Status - 1) {
        LogDbg("Ignore invalid type %u, current %d", MsgPayload.msgbase().msgtype(), clientNode->RegisterCtx.Status);
        return -EEXIST;
    }

    switch (MsgPayload.msgbase().msgtype()) {
        case SC_MSG_TYPE_REGISTER_REQUEST:
            clientNode->ClientId = MsgPayload.msgbase().registerrequest().clientid();
            clientNode->RegisterCtx.TransCipherSuite = MsgPayload.msgbase().registerrequest().ciphersuite();
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
            sendMsgPayload.mutable_msgbase()->set_msgtype(SC_MSG_TYPE_REGISTER_CHALLENGE);
            sendMsgPayload.mutable_msgbase()->mutable_registerchallenge()->set_cipherrand(randTmpBuff, randTmpBuffLen);
            clientNode->RegisterCtx.Status = SC_MSG_TYPE_REGISTER_CHALLENGE;
            break;
        case SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY: {
            auto &receivedPlain = MsgPayload.msgbase().registerchallengereply().plainrand();
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

            auto &receivedCipher = MsgPayload.msgbase().registerchallengereply().cipherrand();
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
            sendMsgPayload.mutable_msgbase()->set_msgtype(SC_MSG_TYPE_REGISTER_FINISH);
            sendMsgPayload.mutable_msgbase()->mutable_registerfinish()->set_plainrand(plainBuff, plainBuffLen);
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
                sendMsgPayload.mutable_msgbase()->
                    mutable_registerfinish()->mutable_ciphercontent()->set_ciphersm4key(randTmpBuff, randTmpBuffLen);
            } else {
                ret = -ENOSYS;
                LogErr("Unsupported crypt suite %d", clientNode->RegisterCtx.TransCipherSuite);
                goto CommRet;
            }
            sendMsgPayload.mutable_msgbase()->
                mutable_registerfinish()->mutable_ciphercontent()->set_ciphersuite(clientNode->RegisterCtx.TransCipherSuite);
            clientNode->RegisterCtx.Status = SC_MSG_TYPE_REGISTER_FINISH;
            worker->ClientCurrentNum.fetch_add(1);
            if (worker->ClientFdMap.find(clientNode->ClientId) == worker->ClientFdMap.end()) {
                worker->ClientFdMap[clientNode->ClientId] = Fd;
                LogInfo("Insert id-fd:%u %d", clientNode->ClientId, Fd);
            }
            break;
        }
        default:
            LogErr("Ignore invalid type %u", MsgPayload.msgbase().msgtype());
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
    auto *worker = ServerWorker;
    auto &transMsg = RecvMsgPayload.msgbase().transmsg();
    uint32_t toClientId = transMsg.to().clientid();
    uint32_t fromClientId = transMsg.from().clientid();
    
    auto peerIt = worker->ClientFdMap.find(toClientId);
    if (peerIt == worker->ClientFdMap.end()) {
        sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg("Peer not registered!");
        ret = -EINVAL;
        LogErr("Peer not registered, clientid %u", toClientId);
        goto CommErr;
    }

    peerFd = peerIt->second;
    auto srcIt = worker->ClientMap.find(Fd);
    if (srcIt == worker->ClientMap.end() ||
        srcIt->second->RegisterCtx.Status != SCMsg::SC_MSG_TYPE_REGISTER_FINISH) {
        sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg("Not registered, please wait or check!");
        ret = -EINVAL;
        LogErr("Sender not registered!");
        goto CommErr;
    }
    auto peerNodeIt = worker->ClientMap.find(peerFd);
    if (peerNodeIt == worker->ClientMap.end() ||
        peerNodeIt->second->RegisterCtx.Status != SCMsg::SC_MSG_TYPE_REGISTER_FINISH) {
        sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg("Peer not registered, please wait or check!");
        ret = -EINVAL;
        LogErr("Peer not registered!");
        goto CommErr;
    }

    sendMsgPayload.mutable_msgbase()->set_msgtype(SCMsg::SC_MSG_TYPE_MSG_TRANS_S_2_C);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_from()->set_clientid(toClientId);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_to()->set_clientid(fromClientId);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->set_msg(transMsg.msg());
    goto CommRet;

CommErr:
    peerFd = Fd;
    sendMsgPayload.set_errcode(ECONNREFUSED);
    sendMsgPayload.mutable_msgbase()->set_msgtype(SCMsg::SC_MSG_TYPE_MSG_TRANS_S_2_C);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_from()->set_clientid(fromClientId);
    sendMsgPayload.mutable_msgbase()->mutable_transmsg()->mutable_to()->set_clientid(fromClientId);

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
    switch (MsgPayload.msgbase().msgtype()) {
        case SC_MSG_TYPE_REGISTER_REQUEST:
        case SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY:
            if (ServerWorker->ClientCurrentNum.load() < ServerWorker->InitParam.Load) {
                ret = RegisterClient(Fd, MsgPayload);
            } else {
                LogWarn("ClientNum has reached its upper limit %u, ignore connect request!", ServerWorker->ClientCurrentNum.load());
                ret = -EBUSY;
            }
            break;
        case SC_MSG_TYPE_MSG_TRANS_C_2_S:
            ret = TransmitMsg(Fd, MsgPayload);
            break;
        default:
            LogErr("Invalid type:%d", MsgPayload.msgbase().msgtype());
            return -EIO;
    }
    return ret;
}

ServerMsgHandler::ServerMsgHandler(ServerWorker* Worker):TransId(0){
    TransId.fetch_add(1);
    ServerMsgHandler::ServerWorker = Worker;
};
ServerMsgHandler::~ServerMsgHandler(){};

