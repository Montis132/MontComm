#include <chrono>

#include "ClientMsgHandler.h"
#include "ClientWorker.h"
#include "SCMsg.h"

using namespace SCMsg;

void ClientMsgHandler::ProtoInitMsg(
    MsgPayload &MsgPayload
    )
{
    UNUSED(MsgPayload.msgBase);
    UNUSED(MsgPayload.clientInfo);
    MsgPayload.transId = TransId.load();
    MsgPayload.clientInfo.clientId = worker->InitParam.ClientId;
}

void ClientMsgHandler::ProtoPreSend(MsgPayload &MsgPayload) {
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = nowMs.time_since_epoch();
    long timestamp = value.count();

    MsgPayload.timestamp = timestamp;
    TransId.fetch_add(1);
}

void ClientMsgHandler::ProtoRelease(MsgPayload &MsgPayload) {
    UNUSED(MsgPayload.msgBase);
    UNUSED(MsgPayload.clientInfo);
}

ERR_T ClientMsgHandler::CreateRegisterProtoMsg(
    MsgBase &MsgBase
    ) 
{
    ERR_T ret = SUCCESS;

    switch (worker->RegisterCtx.Status) {
        case 0:
        case SC_MSG_TYPE_REGISTER_REQUEST:
            MsgBase.msgType = SC_MSG_TYPE_REGISTER_REQUEST;
            MsgBase.registerRequest.clientId = worker->InitParam.ClientId;
            worker->RegisterCtx.TransCipherSuite = worker->InitParam.CryptoSuite;
            MsgBase.registerRequest.cipherSuite = worker->RegisterCtx.TransCipherSuite;
            worker->RegisterCtx.Status = SC_MSG_TYPE_REGISTER_REQUEST;
            break;
        case SC_MSG_TYPE_REGISTER_CHALLENGE:
        case SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY:
        case SC_MSG_TYPE_REGISTER_FINISH:
            ret = -EINVAL;
            break;
    }

    return ret;
}

ERR_T ClientMsgHandler::DispatchMsg(MsgPayload MsgPayload) {
    ERR_T ret = SUCCESS;
    uint8_t randTmpBuff[128];
    size_t randTmpBuffLen = sizeof(randTmpBuff);
    std::string receivedRand;
    uint8_t plainBuff[128];
    size_t plainBuffLen = sizeof(plainBuff);
    SCMsg::MsgPayload sendMsg;
    UTIL_Q_MSG *sendQMsg = NULL;
    
    ProtoInitMsg(sendMsg);


    switch (MsgPayload.msgBase.msgType) {
        case SC_MSG_TYPE_REGISTER_CHALLENGE:
            if (MsgPayload.errCode != 0) {
                worker->RegisterCtx.Status = 0;
                goto CommRet;
            }
            // decrypt cipherrand and send
            receivedRand.assign(MsgPayload.msgBase.registerChallenge.cipherRand.begin(), MsgPayload.msgBase.registerChallenge.cipherRand.end());
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE CipherRand", randTmpBuff, randTmpBuffLen);
            ret = Util_CryptSm2Decrypt(randTmpBuff, randTmpBuffLen, worker->PriKeyPath.c_str(), worker->InitParam.PriKeyPwd.c_str(),
                    plainBuff, &plainBuffLen);
            if (ret) {
                LogErr("Decrypt by prikey failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE CipherRand-Dec", plainBuff, plainBuffLen);
            ret = Util_CryptRandBytes(worker->RegisterCtx.ChallengeRandSend, sizeof(worker->RegisterCtx.ChallengeRandSend));
            if (ret) {
                LogErr("Gen Rand Bytes failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE PlainRand", worker->RegisterCtx.ChallengeRandSend, sizeof(worker->RegisterCtx.ChallengeRandSend));
            randTmpBuffLen = sizeof(randTmpBuff);
            ret = Util_CryptSm2Encrypt(worker->RegisterCtx.ChallengeRandSend, sizeof(worker->RegisterCtx.ChallengeRandSend),
                    &worker->RegisterCtx.PubKey, randTmpBuff, &randTmpBuffLen);
            if (ret) {
                LogErr("Encrypt by pubkey failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE PlainRand-Enc", randTmpBuff, randTmpBuffLen);
            sendMsg.msgBase.msgType = SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY;
            sendMsg.msgBase.registerChallengeReply.plainRand.assign(plainBuff, plainBuff + plainBuffLen);
            sendMsg.msgBase.registerChallengeReply.cipherRand.assign(randTmpBuff, randTmpBuff + randTmpBuffLen);
            ProtoPreSend(sendMsg);
            {
                uint8_t msgpackBuf[4096];
                size_t msgpackLen = SCMsg::MsgPayloadEncodeToBuf(sendMsg, msgpackBuf, sizeof(msgpackBuf));
                sendQMsg = Util_NewSendQMsg(msgpackLen);
                if (!sendQMsg) {
                    goto CommRet;
                }
                memcpy(sendQMsg->Cont.VarLenCont, msgpackBuf, msgpackLen);
            }
            ret = Util_SendQMsg(worker->WorkerFd, sendQMsg);
            if (ret < SUCCESS) {
                LogErr("send msg failed! ret %d", ret);
                goto CommRet;
            }
            LogInfo("Send Msg: %s", SCMsg::MsgPayloadToString(sendMsg).c_str());
            break;
        case SC_MSG_TYPE_REGISTER_FINISH:
            if (MsgPayload.errCode != 0) {
                worker->RegisterCtx.Status = 0;
                goto CommRet;
            }
            // check decrypt rand
            receivedRand.assign(MsgPayload.msgBase.registerFinish.plainRand.begin(), MsgPayload.msgBase.registerFinish.plainRand.end());
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            if (randTmpBuffLen != sizeof(worker->RegisterCtx.ChallengeRandSend) || 
                memcmp(randTmpBuff, worker->RegisterCtx.ChallengeRandSend, randTmpBuffLen) != 0) {
                ret = -EIO;
                LogErr("Random mismatch!");
                goto CommRet;
            }
            Util_Hexdump("REGISTER_FINISH PlainRand", randTmpBuff, randTmpBuffLen);
            // decrypt cipherrand and send
            randTmpBuffLen = sizeof(randTmpBuff);
            receivedRand.assign(MsgPayload.msgBase.registerFinish.cipherContent.cipherSM4Key.begin(), MsgPayload.msgBase.registerFinish.cipherContent.cipherSM4Key.end());
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            if ((uint32_t)worker->RegisterCtx.TransCipherSuite != MsgPayload.msgBase.registerFinish.cipherContent.cipherSuite) {
                ret = -EIO;
                LogErr("Crypto suite mismatch!\n");
                goto CommRet;
            }
            Util_Hexdump("REGISTER_FINISH SM4-Key-Enc", randTmpBuff, randTmpBuffLen);
            if (worker->RegisterCtx.TransCipherSuite == SC_CIPHER_SUITE_SM4) {
                ret = Util_CryptSm2Decrypt(randTmpBuff, randTmpBuffLen, worker->PriKeyPath.c_str(), worker->InitParam.PriKeyPwd.c_str(),
                    plainBuff, &plainBuffLen);
                if (ret || plainBuffLen != sizeof(worker->RegisterCtx.TransKey.Sm4Key)) {
                    ret = -EIO;
                    LogErr("decrypt failed!\n");
                    goto CommRet;
                }
                memcpy(worker->RegisterCtx.TransKey.Sm4Key, plainBuff, plainBuffLen);
                Util_Hexdump("REGISTER_FINISH SM4-Key", plainBuff, plainBuffLen);
            }
            worker->RegisterCtx.Status = SC_MSG_TYPE_REGISTER_FINISH;
            worker->State = C_WORKER_STATS_REGISTERED;
            LogInfo("Register to server success, set status as registered.");
            break;
        case SC_MSG_TYPE_MSG_TRANS_S_2_C:
            std::cout << "Recv from client-" << MsgPayload.msgBase.transMsg.from.clientId << ":" << MsgPayload.msgBase.transMsg.msg << '\n';
            break;
        default:
            ret = -EINVAL;
            goto CommRet;
    }

CommRet:
    ProtoRelease(sendMsg);
    if (sendQMsg) 
        Util_FreeSendQMsg(sendQMsg);
    return ret;
}

ClientMsgHandler::ClientMsgHandler(ClientWorker *Worker):TransId(0){
    TransId.fetch_add(1);
    worker = Worker;
};
ClientMsgHandler::~ClientMsgHandler(){};
