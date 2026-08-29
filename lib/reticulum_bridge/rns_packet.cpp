#include "reticulum/rns_packet.h"
#include <Crypto.h>
#include <SHA256.h>

// Wire layout (matches RNS header byte packing at the bit level):
//   Byte 0: [IFAC:1][HeaderType:1][unused/context-flag:1][PropType:2][DestType:2][unused:1]
//           simplified here to the fields we actually branch on: IFAC (bit7),
//           HeaderType (bit6), DestType (bits 2-1), PacketType folded into
//           context handling below. See docs/RETICULUM_BRIDGE.md for the
//           exact bit assignment and what's simplified vs. full RNS.
//   Byte 1: hop count
//   [16B transport id]  -- only if HeaderType == Type2
//   [16B destination hash]
//   Byte N: context
//   [payload...]

namespace omokoda {

namespace {
constexpr uint8_t kIfacBit = 0x80;
constexpr uint8_t kHeaderTypeBit = 0x40;
constexpr uint8_t kDestTypeMask = 0x06; // bits 2-1
constexpr uint8_t kDestTypeShift = 1;
constexpr uint8_t kPacketTypeMask = 0x01; // low bit, only distinguishes
                                          // data(0) vs non-data(1) at the
                                          // header level; announce/link/proof
                                          // are disambiguated via context —
                                          // this is a deliberate simplification,
                                          // see docs/RETICULUM_BRIDGE.md.
} // namespace

std::vector<uint8_t> RnsPacket::serialize() const {
    std::vector<uint8_t> out;

    uint8_t h0 = 0;
    if (ifacAuthenticated) h0 |= kIfacBit;
    if (headerType == RnsHeaderType::Type2) h0 |= kHeaderTypeBit;
    h0 |= (static_cast<uint8_t>(destType) << kDestTypeShift) & kDestTypeMask;
    if (packetType != RnsPacketType::Data) h0 |= kPacketTypeMask;

    out.push_back(h0);
    out.push_back(hops);

    if (headerType == RnsHeaderType::Type2) {
        out.insert(out.end(), transportId.begin(), transportId.end());
    }
    out.insert(out.end(), destHash.begin(), destHash.end());

    out.push_back(context);
    out.insert(out.end(), payload.begin(), payload.end());

    return out;
}

bool RnsPacket::deserialize(const uint8_t* data, size_t len, RnsPacket& out) {
    if (len < 2 + 16 + 1) return false; // header+hops+desthash+context minimum

    size_t pos = 0;
    uint8_t h0 = data[pos++];
    out.ifacAuthenticated = (h0 & kIfacBit) != 0;
    out.headerType = (h0 & kHeaderTypeBit) ? RnsHeaderType::Type2 : RnsHeaderType::Type1;
    out.destType = static_cast<RnsDestType>((h0 & kDestTypeMask) >> kDestTypeShift);
    // packetType is refined once we know the context byte, below.

    out.hops = data[pos++];

    if (out.headerType == RnsHeaderType::Type2) {
        if (pos + 16 > len) return false;
        std::copy(data + pos, data + pos + 16, out.transportId.begin());
        pos += 16;
    }

    if (pos + 16 > len) return false;
    std::copy(data + pos, data + pos + 16, out.destHash.begin());
    pos += 16;

    if (pos + 1 > len) return false;
    out.context = data[pos++];

    // Refine packet type: bit0 of h0 only says data-vs-not; context
    // disambiguates which non-data type. This mapping is a bridge-local
    // convention, not verified against real RNS context byte semantics —
    // see docs/RETICULUM_BRIDGE.md.
    if (h0 & kPacketTypeMask) {
        switch (out.context) {
            case 1: out.packetType = RnsPacketType::Announce; break;
            case 2: out.packetType = RnsPacketType::LinkRequest; break;
            case 3: out.packetType = RnsPacketType::Proof; break;
            default: out.packetType = RnsPacketType::Data; break;
        }
    } else {
        out.packetType = RnsPacketType::Data;
    }

    out.payload.assign(data + pos, data + len);
    return true;
}

RnsDestHash rnsNameHash(const std::string& appName, const std::vector<std::string>& aspects) {
    std::string full = appName;
    for (const auto& a : aspects) {
        full += ".";
        full += a;
    }

    SHA256 sha;
    sha.update(reinterpret_cast<const uint8_t*>(full.data()), full.size());
    uint8_t digest[32];
    sha.finalize(digest, sizeof(digest));

    RnsDestHash out{};
    std::copy(digest, digest + 16, out.begin()); // RNS truncates to 16 bytes
    return out;
}

} // namespace omokoda
