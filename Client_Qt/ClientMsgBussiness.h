#ifndef _CLIENT_MSG_BUSSINESS_H_
#define _CLIENT_MSG_BUSSINESS_H_

#include <atomic>
#include "SCMsg.pb.h"
#include "UtilsCommonUtil.h"

class ClientWorker;

class ClientMsgHandler{
private:
    std::atomic<uint32_t> TransId;
public:
    ClientMsgHandler();
    ~ClientMsgHandler();
    void ProtoInitMsg(ClientWorker*, SCMsg::MsgPayload&);
    void ProtoPreSend(SCMsg::MsgPayload&);
    void ProtoRelease(SCMsg::MsgPayload&);
    ERR_T DispatchMsg(ClientWorker*, SCMsg::MsgPayload);
    void CreateRegisterProtoMsg(ClientWorker*, SCMsg::MsgBase&);
};

#endif /* _CLIENT_MSG_BUSSINESS_H_ */
