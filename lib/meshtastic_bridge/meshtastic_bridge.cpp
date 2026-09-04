#include "meshtastic/meshtastic_bridge.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace omokoda {

namespace {

constexpr uint8_t kWireVarint = 0;
constexpr uint8_t kWireFixed64 = 1;
constexpr uint8_t kWireLength = 2;
constexpr uint8_t kWireFixed32 = 5;

bool readVarint(const uint8_t* data, size_t len, size_t& pos, uint64_t& out) {
    out = 0;
    unsigned shift = 0;
    while (true) {
        if (pos >= len) return false;
        if (shift > 63) return false;  // longer than 64 bits
        const uint8_t byte = data[pos++];
        out |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if (!(byte & 0x80)) return true;
        shift += 7;
    }
}

bool readFixed32(const uint8_t* data, size_t len, size_t& pos, uint32_t& out) {
    if (pos + 4 > len) return false;
    out = static_cast<uint32_t>(data[pos]) | (static_cast<uint32_t>(data[pos + 1]) << 8) |
          (static_cast<uint32_t>(data[pos + 2]) << 16) |
          (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;
    return true;
}

// Advances past a field this bridge does not model. Skipping by wire type
// rather than by field number is what lets a newer Meshtastic firmware add
// fields without breaking this decoder.
bool skipField(const uint8_t* data, size_t len, size_t& pos, uint8_t wireType) {
    uint64_t scratch = 0;
    switch (wireType) {
        case kWireVarint:
            return readVarint(data, len, pos, scratch);
        case kWireFixed32:
            if (pos + 4 > len) return false;
            pos += 4;
            return true;
        case kWireFixed64:
            if (pos + 8 > len) return false;
            pos += 8;
            return true;
        case kWireLength: {
            if (!readVarint(data, len, pos, scratch)) return false;
            if (pos + scratch > len) return false;
            pos += static_cast<size_t>(scratch);
            return true;
        }
        default:
            return false;  // groups: removed from proto3, and never valid here
    }
}

void putVarint(std::vector<uint8_t>& out, uint64_t value) {
    while (true) {
        uint8_t byte = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        out.push_back(value ? static_cast<uint8_t>(byte | 0x80) : byte);
        if (!value) return;
    }
}

void putKey(std::vector<uint8_t>& out, uint32_t field, uint8_t wireType) {
    putVarint(out, (static_cast<uint64_t>(field) << 3) | wireType);
}

void putFixed32(std::vector<uint8_t>& out, uint32_t field, uint32_t value) {
    putKey(out, field, kWireFixed32);
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
}

void putUint(std::vector<uint8_t>& out, uint32_t field, uint64_t value) {
    putKey(out, field, kWireVarint);
    putVarint(out, value);
}

void putBytes(std::vector<uint8_t>& out, uint32_t field, const uint8_t* data, size_t len) {
    putKey(out, field, kWireLength);
    putVarint(out, len);
    out.insert(out.end(), data, data + len);
}

bool decodeData(const uint8_t* data, size_t len, MeshtasticFrame& out) {
    size_t pos = 0;
    while (pos < len) {
        uint64_t key = 0;
        if (!readVarint(data, len, pos, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t wireType = static_cast<uint8_t>(key & 0x07);
        if (field == 0) return false;

        if (field == kMeshtasticDataPortnum && wireType == kWireVarint) {
            uint64_t value = 0;
            if (!readVarint(data, len, pos, value)) return false;
            out.portnum = static_cast<uint32_t>(value);
        } else if (field == kMeshtasticDataPayload && wireType == kWireLength) {
            uint64_t length = 0;
            if (!readVarint(data, len, pos, length)) return false;
            if (pos + length > len) return false;
            out.payload.assign(data + pos, data + pos + length);
            pos += static_cast<size_t>(length);
        } else if (!skipField(data, len, pos, wireType)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool meshtasticDecode(const uint8_t* data, size_t len, MeshtasticFrame& out) {
    if (data == nullptr || len == 0) return false;
    out = MeshtasticFrame{};

    size_t pos = 0;
    while (pos < len) {
        uint64_t key = 0;
        if (!readVarint(data, len, pos, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t wireType = static_cast<uint8_t>(key & 0x07);
        if (field == 0) return false;

        switch (field) {
            case kMeshtasticFieldFrom:
                if (wireType != kWireFixed32 || !readFixed32(data, len, pos, out.fromNode))
                    return false;
                break;
            case kMeshtasticFieldTo:
                if (wireType != kWireFixed32 || !readFixed32(data, len, pos, out.toNode))
                    return false;
                break;
            case kMeshtasticFieldId:
                if (wireType != kWireFixed32 || !readFixed32(data, len, pos, out.packetId))
                    return false;
                break;
            case kMeshtasticFieldRxTime:
                if (wireType != kWireFixed32 || !readFixed32(data, len, pos, out.rxTime))
                    return false;
                break;
            case kMeshtasticFieldChannel:
            case kMeshtasticFieldHopLimit:
            case kMeshtasticFieldViaMqtt:
            case kMeshtasticFieldHopStart:
            case kMeshtasticFieldRelayNode: {
                if (wireType != kWireVarint) return false;
                uint64_t value = 0;
                if (!readVarint(data, len, pos, value)) return false;
                if (field == kMeshtasticFieldChannel) out.channel = static_cast<uint32_t>(value);
                else if (field == kMeshtasticFieldHopLimit) out.hopLimit = static_cast<uint8_t>(value);
                else if (field == kMeshtasticFieldViaMqtt) out.viaMqtt = value != 0;
                else if (field == kMeshtasticFieldHopStart) out.hopStart = static_cast<uint8_t>(value);
                else out.relayNode = static_cast<uint32_t>(value);
                break;
            }
            case kMeshtasticFieldDecoded: {
                if (wireType != kWireLength) return false;
                uint64_t length = 0;
                if (!readVarint(data, len, pos, length)) return false;
                if (pos + length > len) return false;
                if (!decodeData(data + pos, static_cast<size_t>(length), out)) return false;
                pos += static_cast<size_t>(length);
                break;
            }
            case kMeshtasticFieldEncrypted: {
                // The other arm of the payload_variant oneof. Recorded, not
                // opened: without the channel key there is nothing to read.
                if (wireType != kWireLength) return false;
                uint64_t length = 0;
                if (!readVarint(data, len, pos, length)) return false;
                if (pos + length > len) return false;
                out.encrypted = true;
                pos += static_cast<size_t>(length);
                break;
            }
            default:
                if (!skipField(data, len, pos, wireType)) return false;
                break;
        }
    }

    // Protobuf has no terminator, so a frame cut exactly between fields
    // decodes cleanly as a shorter one. Nothing structural catches that;
    // requiring what a real received packet always carries does.
    if (out.packetId == 0 || out.fromNode == 0) return false;
    if (out.hopLimit > kMeshtasticMaxHopLimit) return false;
    if (out.hopStart != 0 && out.hopLimit > out.hopStart) return false;
    return true;
}

std::vector<uint8_t> meshtasticEncode(const MeshtasticFrame& frame) {
    std::vector<uint8_t> out;
    putFixed32(out, kMeshtasticFieldFrom, frame.fromNode);
    putFixed32(out, kMeshtasticFieldTo, frame.toNode);
    if (frame.channel) putUint(out, kMeshtasticFieldChannel, frame.channel);

    if (frame.encrypted) {
        // Nothing to re-encrypt: the bridge never held the plaintext.
        putBytes(out, kMeshtasticFieldEncrypted, frame.payload.data(), frame.payload.size());
    } else {
        std::vector<uint8_t> data;
        putUint(data, kMeshtasticDataPortnum, frame.portnum);
        putBytes(data, kMeshtasticDataPayload, frame.payload.data(), frame.payload.size());
        putBytes(out, kMeshtasticFieldDecoded, data.data(), data.size());
    }

    putFixed32(out, kMeshtasticFieldId, frame.packetId);
    if (frame.rxTime) putFixed32(out, kMeshtasticFieldRxTime, frame.rxTime);
    if (frame.hopLimit) putUint(out, kMeshtasticFieldHopLimit, frame.hopLimit);
    if (frame.viaMqtt) putUint(out, kMeshtasticFieldViaMqtt, 1);
    if (frame.hopStart) putUint(out, kMeshtasticFieldHopStart, frame.hopStart);
    if (frame.relayNode) putUint(out, kMeshtasticFieldRelayNode, frame.relayNode);
    return out;
}

int meshtasticHopsTaken(const MeshtasticFrame& frame) {
    if (frame.hopStart == 0) return -1;
    if (frame.hopLimit > frame.hopStart) return -1;
    return static_cast<int>(frame.hopStart - frame.hopLimit);
}

std::string meshtasticNodeId(uint32_t nodeNumber) {
    char buffer[12];
    std::snprintf(buffer, sizeof(buffer), "!%08x", nodeNumber);
    return std::string(buffer);
}

Envelope meshtasticToEnvelope(const MeshtasticFrame& frame) {
    Envelope env;
    env.originNetwork = SourceNetwork::Meshtastic;
    env.originId = meshtasticNodeId(frame.packetId);
    // Deliberately empty. `from` is a device-assigned 32-bit number, not a
    // key, and putting it here would let a consumer read a node number as an
    // identity.
    env.originPubkeyHex.clear();
    env.createdAt = frame.rxTime;

    const int hops = meshtasticHopsTaken(frame);
    env.hopCount = hops < 0 ? 0 : static_cast<uint8_t>(hops);
    // Empty, and it stays empty: Meshtastic relays do not sign what they
    // forward, so there is no hop trail to carry.
    env.hopTrail.clear();

    env.payload = frame.payload;
    env.nostrKind = 20000;
    return env;
}

bool envelopeToMeshtastic(const Envelope& env, MeshtasticFrame& out) {
    if (env.originNetwork != SourceNetwork::Meshtastic) return false;
    out = MeshtasticFrame{};

    // originId is "!hhhhhhhh"; anything else did not come from this bridge.
    if (env.originId.size() != 9 || env.originId[0] != '!') return false;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(env.originId.c_str() + 1, &end, 16);
    if (end == nullptr || *end != '\0') return false;

    out.packetId = static_cast<uint32_t>(parsed);
    out.rxTime = env.createdAt;
    out.payload = env.payload;
    // Broadcast: a specific destination node number is not recoverable from
    // an envelope, and guessing one would silently misroute.
    out.toNode = 0xFFFFFFFFu;
    // fromNode is left at 0, and that is not an omission to tidy up later.
    // An Envelope carries no Meshtastic node number -- meshtasticToEnvelope
    // deliberately drops it rather than passing a device-assigned integer off
    // as an identity -- so the only correct value is the transmitting node's
    // own, which this function has no way to know. The caller sets it before
    // transmitting, and meshtasticDecode rejects a frame that still has 0,
    // so forgetting fails loudly rather than putting an unaddressed packet
    // on the air.
    return true;
}

}  // namespace omokoda
