#ifndef SC_MSG_H
#define SC_MSG_H

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <msgpack.hpp>

namespace SCMsg {

enum MsgType : uint32_t {
    SC_MSG_TYPE_UKNOWN                   = 0,
    SC_MSG_TYPE_REGISTER_REQUEST         = 1,
    SC_MSG_TYPE_REGISTER_CHALLENGE       = 2,
    SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY = 3,
    SC_MSG_TYPE_REGISTER_FINISH          = 4,
    SC_MSG_TYPE_MSG_TRANS_C_2_S          = 100,
    SC_MSG_TYPE_MSG_TRANS_S_2_C          = 101,
};

enum CipherSuite : uint32_t {
    SC_CIPHER_SUITE_NONE = 0,
    SC_CIPHER_SUITE_SM4  = 1,
};

struct ClientInfo {
    uint32_t clientId = 0;
};

struct ServerInfo {
    uint32_t serverId = 0;
    std::string serverName;
};

struct ClientRegisterRequest {
    uint32_t clientId = 0;
    uint32_t cipherSuite = 0;
};

struct ServerRegisterChallenge {
    std::vector<uint8_t> cipherRand;
};

struct ClientRegisterChallengeReply {
    std::vector<uint8_t> plainRand;
    std::vector<uint8_t> cipherRand;
};

struct CipherContent {
    uint32_t cipherSuite = 0;
    std::vector<uint8_t> cipherSM4Key;
};

struct ServerRegisterFinish {
    std::vector<uint8_t> plainRand;
    CipherContent cipherContent;
};

struct TransMsg {
    ClientInfo from;
    ClientInfo to;
    std::string msg;
};

struct MsgBase {
    uint32_t msgType = 0;
    ClientRegisterRequest registerRequest;
    ServerRegisterChallenge registerChallenge;
    ClientRegisterChallengeReply registerChallengeReply;
    ServerRegisterFinish registerFinish;
    TransMsg transMsg;
};

struct MsgPayload {
    uint32_t transId = 0;
    uint64_t timestamp = 0;
    std::string bussinessKey;
    ClientInfo clientInfo;
    ServerInfo serverInfo;
    int32_t errCode = 0;
    MsgBase msgBase;
};

template <typename Stream>
static inline void ClientInfoEncode(msgpack::packer<Stream>& pk, const ClientInfo& v) {
    pk.pack_map(1);
    pk.pack_uint8(1); pk.pack_uint32(v.clientId);
}

static inline bool ClientInfoDecode(const msgpack::object& obj, ClientInfo& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.clientId = (uint32_t)map.ptr[i].val.via.u64; break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void ServerInfoEncode(msgpack::packer<Stream>& pk, const ServerInfo& v) {
    pk.pack_map(2);
    pk.pack_uint8(1); pk.pack_uint32(v.serverId);
    pk.pack_uint8(2); pk.pack_raw(v.serverName.size()); pk.pack_raw_body(v.serverName.data(), v.serverName.size());
}

static inline bool ServerInfoDecode(const msgpack::object& obj, ServerInfo& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.serverId = (uint32_t)map.ptr[i].val.via.u64; break;
            case 2: v.serverName.assign(map.ptr[i].val.via.raw.ptr, map.ptr[i].val.via.raw.size); break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void ClientRegisterRequestEncode(msgpack::packer<Stream>& pk, const ClientRegisterRequest& v) {
    pk.pack_map(2);
    pk.pack_uint8(1); pk.pack_uint32(v.clientId);
    pk.pack_uint8(2); pk.pack_uint32(v.cipherSuite);
}

static inline bool ClientRegisterRequestDecode(const msgpack::object& obj, ClientRegisterRequest& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.clientId = (uint32_t)map.ptr[i].val.via.u64; break;
            case 2: v.cipherSuite = (uint32_t)map.ptr[i].val.via.u64; break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void ServerRegisterChallengeEncode(msgpack::packer<Stream>& pk, const ServerRegisterChallenge& v) {
    pk.pack_map(1);
    pk.pack_uint8(1); pk.pack_raw((uint32_t)v.cipherRand.size()); pk.pack_raw_body(reinterpret_cast<const char*>(v.cipherRand.data()), (uint32_t)v.cipherRand.size());
}

static inline bool ServerRegisterChallengeDecode(const msgpack::object& obj, ServerRegisterChallenge& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.cipherRand.assign(map.ptr[i].val.via.raw.ptr, map.ptr[i].val.via.raw.ptr + map.ptr[i].val.via.raw.size); break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void ClientRegisterChallengeReplyEncode(msgpack::packer<Stream>& pk, const ClientRegisterChallengeReply& v) {
    pk.pack_map(2);
    pk.pack_uint8(1); pk.pack_raw((uint32_t)v.plainRand.size()); pk.pack_raw_body(reinterpret_cast<const char*>(v.plainRand.data()), (uint32_t)v.plainRand.size());
    pk.pack_uint8(2); pk.pack_raw((uint32_t)v.cipherRand.size()); pk.pack_raw_body(reinterpret_cast<const char*>(v.cipherRand.data()), (uint32_t)v.cipherRand.size());
}

static inline bool ClientRegisterChallengeReplyDecode(const msgpack::object& obj, ClientRegisterChallengeReply& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.plainRand.assign(map.ptr[i].val.via.raw.ptr, map.ptr[i].val.via.raw.ptr + map.ptr[i].val.via.raw.size); break;
            case 2: v.cipherRand.assign(map.ptr[i].val.via.raw.ptr, map.ptr[i].val.via.raw.ptr + map.ptr[i].val.via.raw.size); break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void CipherContentEncode(msgpack::packer<Stream>& pk, const CipherContent& v) {
    pk.pack_map(2);
    pk.pack_uint8(1); pk.pack_uint32(v.cipherSuite);
    pk.pack_uint8(2); pk.pack_raw((uint32_t)v.cipherSM4Key.size()); pk.pack_raw_body(reinterpret_cast<const char*>(v.cipherSM4Key.data()), (uint32_t)v.cipherSM4Key.size());
}

static inline bool CipherContentDecode(const msgpack::object& obj, CipherContent& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.cipherSuite = (uint32_t)map.ptr[i].val.via.u64; break;
            case 2: v.cipherSM4Key.assign(map.ptr[i].val.via.raw.ptr, map.ptr[i].val.via.raw.ptr + map.ptr[i].val.via.raw.size); break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void ServerRegisterFinishEncode(msgpack::packer<Stream>& pk, const ServerRegisterFinish& v) {
    pk.pack_map(2);
    pk.pack_uint8(1); pk.pack_raw((uint32_t)v.plainRand.size()); pk.pack_raw_body(reinterpret_cast<const char*>(v.plainRand.data()), (uint32_t)v.plainRand.size());
    pk.pack_uint8(2); CipherContentEncode(pk, v.cipherContent);
}

static inline bool ServerRegisterFinishDecode(const msgpack::object& obj, ServerRegisterFinish& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.plainRand.assign(map.ptr[i].val.via.raw.ptr, map.ptr[i].val.via.raw.ptr + map.ptr[i].val.via.raw.size); break;
            case 2: CipherContentDecode(map.ptr[i].val, v.cipherContent); break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void TransMsgEncode(msgpack::packer<Stream>& pk, const TransMsg& v) {
    pk.pack_map(3);
    pk.pack_uint8(1); ClientInfoEncode(pk, v.from);
    pk.pack_uint8(2); ClientInfoEncode(pk, v.to);
    pk.pack_uint8(3); pk.pack_raw((uint32_t)v.msg.size()); pk.pack_raw_body(v.msg.data(), (uint32_t)v.msg.size());
}

static inline bool TransMsgDecode(const msgpack::object& obj, TransMsg& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: ClientInfoDecode(map.ptr[i].val, v.from); break;
            case 2: ClientInfoDecode(map.ptr[i].val, v.to); break;
            case 3: v.msg.assign(map.ptr[i].val.via.raw.ptr, map.ptr[i].val.via.raw.size); break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void MsgBaseEncode(msgpack::packer<Stream>& pk, const MsgBase& v) {
    uint32_t count = 1;
    if (v.msgType != SC_MSG_TYPE_UKNOWN) count = 2;
    pk.pack_map(count);
    pk.pack_uint8(1); pk.pack_uint32(v.msgType);
    switch (v.msgType) {
        case SC_MSG_TYPE_REGISTER_REQUEST:
            pk.pack_uint8(2); ClientRegisterRequestEncode(pk, v.registerRequest);
            break;
        case SC_MSG_TYPE_REGISTER_CHALLENGE:
            pk.pack_uint8(3); ServerRegisterChallengeEncode(pk, v.registerChallenge);
            break;
        case SC_MSG_TYPE_REGISTER_CHALLENGE_REPLY:
            pk.pack_uint8(4); ClientRegisterChallengeReplyEncode(pk, v.registerChallengeReply);
            break;
        case SC_MSG_TYPE_REGISTER_FINISH:
            pk.pack_uint8(5); ServerRegisterFinishEncode(pk, v.registerFinish);
            break;
        case SC_MSG_TYPE_MSG_TRANS_C_2_S:
        case SC_MSG_TYPE_MSG_TRANS_S_2_C:
            pk.pack_uint8(6); TransMsgEncode(pk, v.transMsg);
            break;
        default:
            break;
    }
}

static inline bool MsgBaseDecode(const msgpack::object& obj, MsgBase& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.msgType = (uint32_t)map.ptr[i].val.via.u64; break;
            case 2: ClientRegisterRequestDecode(map.ptr[i].val, v.registerRequest); break;
            case 3: ServerRegisterChallengeDecode(map.ptr[i].val, v.registerChallenge); break;
            case 4: ClientRegisterChallengeReplyDecode(map.ptr[i].val, v.registerChallengeReply); break;
            case 5: ServerRegisterFinishDecode(map.ptr[i].val, v.registerFinish); break;
            case 6: TransMsgDecode(map.ptr[i].val, v.transMsg); break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void MsgPayloadEncode(msgpack::packer<Stream>& pk, const MsgPayload& v) {
    pk.pack_map(7);
    pk.pack_uint8(1); pk.pack_uint32(v.transId);
    pk.pack_uint8(2); pk.pack_uint64(v.timestamp);
    pk.pack_uint8(3); pk.pack_raw((uint32_t)v.bussinessKey.size()); pk.pack_raw_body(v.bussinessKey.data(), (uint32_t)v.bussinessKey.size());
    pk.pack_uint8(4); ClientInfoEncode(pk, v.clientInfo);
    pk.pack_uint8(5); ServerInfoEncode(pk, v.serverInfo);
    pk.pack_uint8(6); pk.pack_int32(v.errCode);
    pk.pack_uint8(7); MsgBaseEncode(pk, v.msgBase);
}

static inline bool MsgPayloadDecode(const msgpack::object& obj, MsgPayload& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.transId = (uint32_t)map.ptr[i].val.via.u64; break;
            case 2: v.timestamp = map.ptr[i].val.via.u64; break;
            case 3: v.bussinessKey.assign(map.ptr[i].val.via.raw.ptr, map.ptr[i].val.via.raw.size); break;
            case 4: ClientInfoDecode(map.ptr[i].val, v.clientInfo); break;
            case 5: ServerInfoDecode(map.ptr[i].val, v.serverInfo); break;
            case 6: v.errCode = (int32_t)map.ptr[i].val.via.i64; break;
            case 7: MsgBaseDecode(map.ptr[i].val, v.msgBase); break;
            default: break;
        }
    }
    return true;
}

static inline size_t MsgPayloadEncodeToBuf(const MsgPayload& msg, uint8_t* buf, size_t bufSize) {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> pk(sbuf);
    MsgPayloadEncode(pk, msg);
    if (sbuf.size() <= bufSize) {
        memcpy(buf, sbuf.data(), sbuf.size());
    }
    return sbuf.size();
}

static inline bool MsgPayloadDecodeFromBuf(MsgPayload& msg, const uint8_t* buf, size_t bufSize) {
    msgpack::unpacked result;
    msgpack::unpack(&result, (const char*)buf, bufSize);
    return MsgPayloadDecode(result.get(), msg);
}

static inline std::string MsgPayloadToString(const MsgPayload& msg) {
    return "MsgPayload{transId=" + std::to_string(msg.transId) +
           " timestamp=" + std::to_string(msg.timestamp) +
           " msgType=" + std::to_string(msg.msgBase.msgType) + "}";
}

static inline void ProtoInitMsg(MsgPayload& msg, uint32_t msgType) {
    msg.transId = 0;
    msg.timestamp = 0;
    msg.errCode = 0;
    msg.msgBase.msgType = msgType;
}

static inline void ProtoPreSend(MsgPayload& msg) {
    msg.timestamp = (uint64_t)time(nullptr);
}

static inline void ProtoRelease(MsgPayload& msg) {
    (void)msg;
}

} // namespace SCMsg

#endif // SC_MSG_H
