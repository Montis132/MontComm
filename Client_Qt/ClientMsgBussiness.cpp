#include <chrono>

#include "ClientMsgBussiness.h"
#include "ClientWorker.h"

using namespace SCMsg;

void ClientMsgHandler::ProtoInitMsg(
    ClientWorker* Worker,
    MsgPayload &MsgPayload
    )
{
    assert(NULL != Worker);
    assert(NULL != MsgPayload.mutable_msgbase());
    assert(NULL != MsgPayload.mutable_clientinfo());
    MsgPayload.set_transid(TransId.load());
    MsgPayload.mutable_clientinfo()->set_clientid(Worker->InitParam.ClientId);
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
    assert(NULL != MsgPayload.release_msgbase());
    assert(NULL != MsgPayload.release_clientinfo());
}

void ClientMsgHandler::CreateRegisterProtoMsg(
    ClientWorker* Worker,
    MsgBase &MsgBase
    ) 
{
    MsgBase.set_msgtype(SC_MSG_TYPE_REGISTER);
    MsgBase.mutable_clientregister()->set_clientid(Worker->InitParam.ClientId);
}

ERR_T ClientMsgHandler::DispatchMsg(ClientWorker* Worker, MsgPayload MsgPayload) {
    switch (MsgPayload.msgbase().msgtype()) {
        case SC_MSG_TYPE_REGISTER_REPLY:
            if (MsgPayload.msgbase().clientregisterreply().errcode() == SUCCESS) {
                Worker->State = C_WORKER_STATS_REGISTERED;
                Worker->RegisterRetried = 0;
            } else {
                LogErr("Register replies as fail! errcode %d", MsgPayload.msgbase().clientregisterreply().errcode());
            } 
            break;
        default:
            break;
    }

    return SUCCESS;
}

ClientMsgHandler::ClientMsgHandler():TransId(0){
    TransId.fetch_add(1);
};
ClientMsgHandler::~ClientMsgHandler(){};
