#ifndef _CLIENT_MSG_BUSSINESS_H_
#define _CLIENT_MSG_BUSSINESS_H_

#include <atomic>
#include <QObject>
#include "SCMsg.h"
#include "UtilsCommonUtil.h"

class ClientWorker;

class ClientMsgHandler : public QObject{
    Q_OBJECT
private:
    std::atomic<uint32_t> TransId;
public:
    explicit ClientMsgHandler(QObject *parent = nullptr);
    ~ClientMsgHandler();
    void ProtoInitMsg(ClientWorker*, SCMsg::MsgPayload&);
    void ProtoPreSend(SCMsg::MsgPayload&);
    void ProtoRelease(SCMsg::MsgPayload&);
    ERR_T DispatchMsg(ClientWorker*, SCMsg::MsgPayload);
    void CreateRegisterProtoMsg(ClientWorker*, SCMsg::MsgBase&);

signals:
    void stateChanged(int state);
    void msgReceived(uint32_t fromClientId, QString msg);
    void errorOccurred(QString error);
    void registered(uint32_t clientId);
};

#endif /* _CLIENT_MSG_BUSSINESS_H_ */
