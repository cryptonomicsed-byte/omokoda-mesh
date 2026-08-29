#include "reticulum/reticulum_bridge.h"
#include <cstdio>

namespace omokoda {

namespace {

std::string hashToHex(const RnsDestHash& h) {
    static const char* hexChars = "0123456789abcdef";
    std::string out;
    out.reserve(32);
    for (uint8_t b : h) {
        out.push_back(hexChars[(b >> 4) & 0xF]);
        out.push_back(hexChars[b & 0xF]);
    }
    return out;
}

bool hexToHash(const std::string& hex, RnsDestHash& out) {
    if (hex.size() != 32) return false;
    for (size_t i = 0; i < 16; ++i) {
        unsigned int byte;
        if (sscanf(hex.c_str() + i * 2, "%2x", &byte) != 1) return false;
        out[i] = static_cast<uint8_t>(byte);
    }
    return true;
}

} // namespace

bool rnsPacketToEnvelope(const RnsPacket& pkt, Envelope& out) {
    out.originNetwork = SourceNetwork::Reticulum;
    out.originId = hashToHex(pkt.destHash);
    out.originPubkeyHex.clear(); // Reticulum carries no Nostr identity by
                                  // itself; caller must attach one from
                                  // context (e.g. this node's own key when
                                  // originating, or left blank when purely
                                  // relaying an opaque foreign packet).
    out.createdAt = 0; // RNS packets carry no timestamp field at this layer
    out.hopCount = pkt.hops;
    out.nostrKind = 20000; // Mesh Packet Route by default; announce/link
                            // traffic could map to other kinds later if a
                            // concrete need shows up.
    out.payload = pkt.payload;

    return !out.payload.empty() || pkt.packetType == RnsPacketType::Announce;
}

bool envelopeToRnsPacket(const Envelope& env, const std::string& bridgeAppName,
                          const std::vector<std::string>& bridgeAspects,
                          RnsPacket& out) {
    out = RnsPacket{};
    out.hops = env.hopCount;
    out.payload = env.payload;
    out.packetType = RnsPacketType::Data;

    if (env.originNetwork == SourceNetwork::Reticulum && !env.originId.empty()) {
        if (!hexToHash(env.originId, out.destHash)) return false;
    } else {
        // Bridging a non-Reticulum envelope onto a Reticulum interface:
        // derive a destination hash under this bridge's own app
        // name/aspects so it's routable, rather than inventing an
        // identity-keyed destination we have no key material for.
        out.destHash = rnsNameHash(bridgeAppName, bridgeAspects);
    }

    return true;
}

} // namespace omokoda
