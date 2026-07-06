#include <chrono>

#include "ClientMsgBussiness.h"
#include "ClientWorker.h"
#include "SCMsg.h"

using namespace SCMsg;

void ClientMsgHandler::ProtoInitMsg(
    ClientWorker* Worker,
    MsgPayload &MsgPayload
    )
{
    assert(NULL != Worker);
    MsgPayload.transId = TransId.load();
    MsgPayload.clientInfo.clientId = Worker->InitParam.ClientId;
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
}

void ClientMsgHandler::CreateRegisterProtoMsg(
    ClientWorker* Worker,
    MsgBase &MsgBase
    ) 
{
    MsgBase.msgType = SC_MSG_TYPE_REGISTER_REQUEST;
    MsgBase.registerRequest.clientId = Worker->InitParam.ClientId;
}

ERR_T ClientMsgHandler::DispatchMsg(ClientWorker* Worker, MsgPayload MsgPayload) {
    switch (MsgPayload.msgBase.msgType) {
        case SC_MSG_TYPE_REGISTER_REPLY:
            if (MsgPayload.errCode == SUCCESS) {
                Worker->State = C_WORKER_STATS_REGISTERED;
                Worker->RegisterRetried = 0;
                emit registered(Worker->InitParam.ClientId);
                emit stateChanged(C_WORKER_STATS_REGISTERED);
            } else {
                emit errorOccurred(QString("Register failed: errCode %1").arg(MsgPayload.errCode));
                LogErr("Register replies as fail! errcode %d", MsgPayload.errCode);
            } 
            break;
        case SC_MSG_TYPE_MSG_TRANS_S_2_C:
            emit msgReceived(MsgPayload.msgBase.transMsg.from.clientId,
                QString::fromStdString(MsgPayload.msgBase.transMsg.msg));
            break;
        default:
            break;
    }

    return SUCCESS;
}

ClientMsgHandler::ClientMsgHandler(QObject *parent):QObject(parent), TransId(0){
    TransId.fetch_add(1);
};
ClientMsgHandler::~ClientMsgHandler(){};
