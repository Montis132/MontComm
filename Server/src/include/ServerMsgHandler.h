#ifndef _SERVER_MSG_HANDLER_H_
#define _SERVER_MSG_HANDLER_H_

#include <atomic>
#include "SCMsg.h"
#include "UtilsCommonUtil.h"

class ServerWorker;

class ServerMsgHandler{
private:
    std::atomic<uint32_t> TransId;
    ERR_T RegisterClient(int32_t, const SCMsg::MsgPayload);
    ERR_T SendMsgAsync(const SCMsg::MsgPayload, int32_t);
    ERR_T TransmitMsg(int32_t, const SCMsg::MsgPayload);
    class ServerWorker* Worker;
public:
    ServerMsgHandler(ServerWorker*);
    ~ServerMsgHandler();
    void ProtoInitMsg(SCMsg::MsgPayload&);
    void ProtoPreSend(SCMsg::MsgPayload&);
    void ProtoRelease(SCMsg::MsgPayload&);
    ERR_T DispatchMsg(int32_t, const SCMsg::MsgPayload);
};

#endif /* _SERVER_MSG_HANDLER_H_ */
