#include <chrono>

#include "ClientMsgHandler.h"
#include "ClientWorker.h"

using namespace SCMsg;

void ClientMsgHandler::ProtoInitMsg(
    MsgPayload &MsgPayload
    )
{
    UNUSED(MsgPayload.mutable_msgbase());
    UNUSED(MsgPayload.mutable_clientinfo());
    MsgPayload.set_transid(TransId.load());
    MsgPayload.mutable_clientinfo()->set_clientid(ClientWorker->InitParam.ClientId);
}

void ClientMsgHandler::ProtoPreSend(MsgPayload &MsgPayload) {
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = nowMs.time_since_epoch();
    long timestamp = value.count();

    MsgPayload.set_timestamp(timestamp);
    TransId.fetch_add(1);
}

void ClientMsgHandler::ProtoRelease(MsgPayload &MsgPayload) {
    UNUSED(MsgPayload.release_msgbase());
    UNUSED(MsgPayload.release_clientinfo());
}

ERR_T ClientMsgHandler::CreateRegisterProtoMsg(
    MsgBase &MsgBase
    ) 
{
    ERR_T ret = SUCCESS;

    switch (ClientWorker->RegisterCtx.Status) {
        case 0:
        case SC_MSG_TYPE_REGISTER_REQUEST:
            MsgBase.set_msgtype(SC_MSG_TYPE_REGISTER_REQUEST);
            MsgBase.mutable_registerrequest()->set_clientid(ClientWorker->InitParam.ClientId);
            ClientWorker->RegisterCtx.TransCipherSuite = ClientWorker->InitParam.CryptoSuite;
            MsgBase.mutable_registerrequest()->set_ciphersuite(ClientWorker->RegisterCtx.TransCipherSuite);
            ClientWorker->RegisterCtx.Status = SC_MSG_TYPE_REGISTER_REQUEST;
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
    std::string serializedData;
    uint8_t plainBuff[128];
    size_t plainBuffLen = sizeof(plainBuff);
    SCMsg::MsgPayload sendMsg;
    UTIL_Q_MSG *sendQMsg = NULL;
    
    ProtoInitMsg(sendMsg);


    switch (MsgPayload.msgbase().msgtype()) {
        case SC_MSG_TYPE_REGISTER_CHALLENGE:
            if (MsgPayload.errcode() != 0) {
                ClientWorker->RegisterCtx.Status = 0;
                goto CommRet;
            }
            // decrypt cipherrand and send
            receivedRand = MsgPayload.msgbase().registerchallenge().cipherrand();
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE CipherRand", randTmpBuff, randTmpBuffLen);
            ret = Util_CryptSm2Decrypt(randTmpBuff, randTmpBuffLen, ClientWorker->PriKeyPath.c_str(), ClientWorker->InitParam.PriKeyPwd.c_str(),
                    plainBuff, &plainBuffLen);
            if (ret) {
                LogErr("Decrypt by prikey failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE CipherRand-Dec", plainBuff, plainBuffLen);
            ret = Util_CryptRandBytes(ClientWorker->RegisterCtx.ChallengeRandSend, sizeof(ClientWorker->RegisterCtx.ChallengeRandSend));
            if (ret) {
                LogErr("Gen Rand Bytes failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE PlainRand", ClientWorker->RegisterCtx.ChallengeRandSend, sizeof(ClientWorker->RegisterCtx.ChallengeRandSend));
            randTmpBuffLen = sizeof(randTmpBuff);
            ret = Util_CryptSm2Encrypt(ClientWorker->RegisterCtx.ChallengeRandSend, sizeof(ClientWorker->RegisterCtx.ChallengeRandSend),
                    &ClientWorker->RegisterCtx.PubKey, randTmpBuff, &randTmpBuffLen);
            if (ret) {
                LogErr("Encrypt by pubkey failed! ret %d", ret);
                goto CommRet;
            }
            Util_Hexdump("REGISTER_CHALLENGE PlainRand-Enc", randTmpBuff, randTmpBuffLen);
            sendMsg.mutable_msgbase()->set_msgtype(SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY);
            sendMsg.mutable_msgbase()->mutable_registerchallengereply()->set_plainrand(plainBuff, plainBuffLen);
            sendMsg.mutable_msgbase()->mutable_registerchallengereply()->set_cipherrand(randTmpBuff, randTmpBuffLen);
            ProtoPreSend(sendMsg);
            serializedData = sendMsg.SerializeAsString();
            sendQMsg = Util_NewSendQMsg(serializedData.size());
            if (!sendQMsg) {
                goto CommRet;
            }
            memcpy(sendQMsg->Cont.VarLenCont, serializedData.data(), serializedData.size());
            ret = Util_SendQMsg(ClientWorker->WorkerFd, sendQMsg);
            if (ret < SUCCESS) {
                LogErr("send msg failed! ret %d", ret);
                goto CommRet;
            }
            LogInfo("Send Msg: %s", sendMsg.ShortDebugString().c_str());
            break;
        case SC_MSG_TYPE_REGISTER_FINISH:
            if (MsgPayload.errcode() != 0) {
                ClientWorker->RegisterCtx.Status = 0;
                goto CommRet;
            }
            // check decrypt rand
            receivedRand = MsgPayload.msgbase().registerfinish().plainrand();
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            if (randTmpBuffLen != sizeof(ClientWorker->RegisterCtx.ChallengeRandSend) || 
                memcmp(randTmpBuff, ClientWorker->RegisterCtx.ChallengeRandSend, randTmpBuffLen) != 0) {
                ret = -EIO;
                LogErr("Random mismatch!");
                goto CommRet;
            }
            Util_Hexdump("REGISTER_FINISH PlainRand", randTmpBuff, randTmpBuffLen);
            // decrypt cipherrand and send
            randTmpBuffLen = sizeof(randTmpBuff);
            receivedRand = MsgPayload.msgbase().registerfinish().ciphercontent().ciphersm4key();
            if (receivedRand.size() <= randTmpBuffLen) {
                std::copy(receivedRand.begin(), receivedRand.end(), randTmpBuff);
                randTmpBuffLen = receivedRand.size();
            } else {
                ret = -E2BIG;
                LogErr("Too big string, size %zu", receivedRand.size());
                goto CommRet;
            }
            if (ClientWorker->RegisterCtx.TransCipherSuite != (int)MsgPayload.msgbase().registerfinish().ciphercontent().ciphersuite()) {
                ret = -EIO;
                LogErr("Crypto suite mismatch!\n");
                goto CommRet;
            }
            Util_Hexdump("REGISTER_FINISH SM4-Key-Enc", randTmpBuff, randTmpBuffLen);
            if (ClientWorker->RegisterCtx.TransCipherSuite == SC_CIPHER_SUITE_SM4) {
                ret = Util_CryptSm2Decrypt(randTmpBuff, randTmpBuffLen, ClientWorker->PriKeyPath.c_str(), ClientWorker->InitParam.PriKeyPwd.c_str(),
                    plainBuff, &plainBuffLen);
                if (ret || plainBuffLen != sizeof(ClientWorker->RegisterCtx.TransKey.Sm4Key)) {
                    ret = -EIO;
                    LogErr("decrypt failed!\n");
                    goto CommRet;
                }
                memcpy(ClientWorker->RegisterCtx.TransKey.Sm4Key, plainBuff, plainBuffLen);
                Util_Hexdump("REGISTER_FINISH SM4-Key", plainBuff, plainBuffLen);
            }
            ClientWorker->RegisterCtx.Status = SC_MSG_TYPE_REGISTER_FINISH;
            ClientWorker->State = C_WORKER_STATS_REGISTERED;
            LogInfo("Register to server success, set status as registered.");
            break;
        case SC_MSG_TYPE_MSG_TRANS_S_2_C:
            std::cout << "Recv from client-" << MsgPayload.msgbase().transmsg().from().clientid() << ":" << MsgPayload.msgbase().transmsg().msg() << '\n';
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
    ClientMsgHandler::ClientWorker = Worker;
};
ClientMsgHandler::~ClientMsgHandler(){};
