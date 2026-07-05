#ifndef _CLIENT_MSG_HANDLER_H_
#define _CLIENT_MSG_HANDLER_H_

#include <atomic>
#include "SCMsg.pb.h"
#include "UtilsCommonUtil.h"

class ClientWorker;

class ClientMsgHandler{
private:
    std::atomic<uint32_t> TransId;
    ClientWorker* ClientWorker;
public:
    ClientMsgHandler(ClientWorker*);
    ~ClientMsgHandler();
    void ProtoInitMsg(SCMsg::MsgPayload&);
    void ProtoPreSend(SCMsg::MsgPayload&);
    void ProtoRelease(SCMsg::MsgPayload&);
    ERR_T DispatchMsg(SCMsg::MsgPayload);
    ERR_T CreateRegisterProtoMsg(SCMsg::MsgBase&);
};

#endif /* _CLIENT_MSG_HANDLER_H_ */
