#ifndef MS_MSG_H
#define MS_MSG_H

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <msgpack.hpp>

namespace MSMsg {

enum MsgType : uint32_t {
    MS_MSG_TYPE_UKNOWN             = 0,
    MS_MSG_TYPE_SVR_HEALTH_REPORT  = 1,
    MS_MSG_TYPE_STOP_WORKER        = 3,
    MS_MSG_TYPE_STOP_WORKER_REPLY  = 4,
    MS_MSG_TYPE_START_WORKER       = 5,
    MS_MSG_TYPE_START_WORKER_REPLY = 6,
};

struct ClientInfo {
    uint32_t clientId = 0;
};

struct ServerInfo {
    uint32_t serverId = 0;
    std::string serverName;
};

struct SvrHealthReport {
    float cpuUsage = 0.0f;
    float memUsage = 0.0f;
    std::vector<ClientInfo> clientInfo;
};

struct MsgBase {
    uint32_t msgType = 0;
    SvrHealthReport svrHealthReport;
};

struct MsgPayload {
    uint32_t transId = 0;
    uint64_t timestamp = 0;
    std::string bussinessKey;
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
static inline void SvrHealthReportEncode(msgpack::packer<Stream>& pk, const SvrHealthReport& v) {
    pk.pack_map(3);
    pk.pack_uint8(1); pk.pack_float(v.cpuUsage);
    pk.pack_uint8(2); pk.pack_float(v.memUsage);
    pk.pack_uint8(3);
    pk.pack_array((uint32_t)v.clientInfo.size());
    for (const auto& ci : v.clientInfo) {
        ClientInfoEncode(pk, ci);
    }
}

static inline bool SvrHealthReportDecode(const msgpack::object& obj, SvrHealthReport& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.cpuUsage = (float)map.ptr[i].val.via.dec; break;
            case 2: v.memUsage = (float)map.ptr[i].val.via.dec; break;
            case 3: {
                auto arr = map.ptr[i].val.via.array;
                v.clientInfo.resize(arr.size);
                for (uint32_t j = 0; j < arr.size; j++) {
                    ClientInfoDecode(arr.ptr[j], v.clientInfo[j]);
                }
                break;
            }
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void MsgBaseEncode(msgpack::packer<Stream>& pk, const MsgBase& v) {
    if (v.msgType == MS_MSG_TYPE_SVR_HEALTH_REPORT) {
        pk.pack_map(2);
        pk.pack_uint8(1); pk.pack_uint32(v.msgType);
        pk.pack_uint8(2); SvrHealthReportEncode(pk, v.svrHealthReport);
    } else {
        pk.pack_map(1);
        pk.pack_uint8(1); pk.pack_uint32(v.msgType);
    }
}

static inline bool MsgBaseDecode(const msgpack::object& obj, MsgBase& v) {
    auto map = obj.via.map;
    for (uint32_t i = 0; i < map.size; i++) {
        auto key = map.ptr[i].key.via.u64;
        switch (key) {
            case 1: v.msgType = (uint32_t)map.ptr[i].val.via.u64; break;
            case 2: SvrHealthReportDecode(map.ptr[i].val, v.svrHealthReport); break;
            default: break;
        }
    }
    return true;
}

template <typename Stream>
static inline void MsgPayloadEncode(msgpack::packer<Stream>& pk, const MsgPayload& v) {
    pk.pack_map(6);
    pk.pack_uint8(1); pk.pack_uint32(v.transId);
    pk.pack_uint8(2); pk.pack_uint64(v.timestamp);
    pk.pack_uint8(3); pk.pack_raw(v.bussinessKey.size()); pk.pack_raw_body(v.bussinessKey.data(), v.bussinessKey.size());
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

} // namespace MSMsg

#endif // MS_MSG_H
