#ifndef _UTIL_MSG_H_
#define _UTIL_MSG_H_
#include "CommonInclude.h"

#ifdef __cplusplus
extern "C"{
#endif

#define UTIL_MSG_CONTENT_MAX_LEN                                    (1024 * 1024)
#define UTIL_MSG_VER(_M_VER_, _F_VER_, _R_VER_)                     (_M_VER_ << 5 | _F_VER_ << 2 | _R_VER_)
                                                    // m_ver 3bits max 7, f_ver 2bits max 3, r_ver 3 bits max 7
#define UTIL_MSG_VER_MAGIC                                          UTIL_MSG_VER(1, 0, 0)

// total 40 Bytes
typedef struct _UTIL_MSG_HEAD{
    uint8_t VerMagic;               // must be the first
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t IsMsgEnd : 1;
    uint8_t Reserved_1 : 7;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t Reserved_1 : 7;
    uint8_t IsMsgEnd : 1;
#else
    #error "unknown byte order!"
#endif
    uint16_t Type;
    uint32_t ContentLen;
    uint32_t SessionId;
    uint32_t ClientId;
    uint8_t Reserved_2[24];
}
UTIL_MSG_HEAD;

typedef struct _UTIL_MSG_CONT
{
    uint8_t VarLenCont[UTIL_MSG_CONTENT_MAX_LEN];
}
UTIL_MSG_CONT;

// total 88 Bytes
typedef struct _UTIL_MSG_TAIL
{
    uint8_t Reserved[16];
    uint64_t TimeStamp;         // ms timestamp
    uint8_t Sign[64];
}
UTIL_MSG_TAIL;
// head + tail = 128 Bytes

typedef struct _UTIL_MSG
{
    UTIL_MSG_HEAD Head;
    UTIL_MSG_CONT Cont;
    UTIL_MSG_TAIL Tail;
}
UTIL_MSG;

typedef struct _UTIL_Q_MSG_HEAD{
    uint32_t ContentLen;
}
UTIL_Q_MSG_HEAD;

typedef struct _UTIL_Q_MSG_CONT
{
    uint8_t *VarLenCont;
}
UTIL_Q_MSG_CONT;

typedef struct _UTIL_Q_MSG
{
    UTIL_Q_MSG_HEAD Head;
    UTIL_Q_MSG_CONT Cont;
}
UTIL_Q_MSG;

int
Util_MsgModuleInit(
    void
    );

int
Util_MsgModuleExit(
    void
    );

int
Util_MsgModuleCollectStat(
    char* Buff,
    int BuffMaxLen,
    int* Offset
    );

int
Util_RecvMsg(
    int Fd,
     UTIL_MSG *RetMsg
    );

MUST_CHECK
UTIL_MSG *
Util_NewMsg(
    void
    );

void
Util_FreeMsg(
    UTIL_MSG *Msg
    );

int
Util_SendMsg(
    int Fd,
    UTIL_MSG *Msg
    );

int
Util_FillMsgCont(
    UTIL_MSG *Msg,
    void* FillCont,
    size_t FillContLen
    );

void
Util_ClearMsgCont(
    UTIL_MSG *Msg
    );

int
Util_RecvQMsg(
    int Fd,
     UTIL_Q_MSG *RetMsg
    );

MUST_CHECK
UTIL_Q_MSG *
Util_NewSendQMsg(
    uint32_t ContLen
    );

void
Util_FreeSendQMsg(
    UTIL_Q_MSG *Msg
    );

int
Util_SendQMsg(
    int Fd,
    UTIL_Q_MSG *Msg
    );
MUST_CHECK
UTIL_Q_MSG* 
Util_NewRecvQMsg(
    void
    );
void
Util_FreeRecvQMsg(
    UTIL_Q_MSG *Msg
    );
    void
Util_FreeRecvQMsgCont(
    UTIL_Q_MSG *Msg
    );

#define UTIL_MSG_FRAGMENT_SIZE  (64 * 1024)

int
Util_SendQMsgFragmented(
    int Fd,
    UTIL_Q_MSG *Msg
    );

UTIL_Q_MSG*
Util_RecvQMsgFragmented(
    int Fd
    );

void
Util_MsgFragCleanup(
    void
    );
#ifdef __cplusplus
}
#endif

#endif /* _UTIL_MSG_H_ */
