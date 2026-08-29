#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <string>

// Reticulum Network Stack (RNS) wire packet — structural model used to
// bridge between RNS and omokoda::Envelope. Field layout follows the
// public RNS packet spec (header flags byte, hop count, 16-byte truncated
// destination hash, context byte, payload) but has NOT been validated
// against a real `rnsd`/RNode interop test yet — see
// docs/RETICULUM_BRIDGE.md "known gaps" before relying on this for actual
// interop with a live Reticulum network. Treat this as the structural
// skeleton, not a spec-certified implementation.

namespace omokoda {

// RNS destination hashes are 16 bytes (128 bits) — a truncated SHA256 of
// (app_name + aspects) for a named destination, or of an identity's public
// key for a single/link destination.
using RnsDestHash = std::array<uint8_t, 16>;

enum class RnsHeaderType : uint8_t {
    Type1 = 0, // destination hash only
    Type2 = 1, // includes a transport-node id ahead of the destination hash
};

enum class RnsDestType : uint8_t {
    Single = 0,
    Group = 1,
    Plain = 2,
    Link = 3,
};

enum class RnsPacketType : uint8_t {
    Data = 0,
    Announce = 1,
    LinkRequest = 2,
    Proof = 3,
};

struct RnsPacket {
    bool ifacAuthenticated = false;
    RnsHeaderType headerType = RnsHeaderType::Type1;
    RnsDestType destType = RnsDestType::Single;
    RnsPacketType packetType = RnsPacketType::Data;
    uint8_t hops = 0;
    RnsDestHash transportId{}; // only meaningful when headerType == Type2
    RnsDestHash destHash{};
    uint8_t context = 0;
    std::vector<uint8_t> payload;

    std::vector<uint8_t> serialize() const;
    static bool deserialize(const uint8_t* data, size_t len, RnsPacket& out);
};

// Truncated SHA256 destination hash, per RNS convention: sha256(name_hash
// || app_data)[:16] for named destinations. This helper covers the common
// "app_name.aspects" form; identity-keyed destinations (single/link) hash
// the identity public key instead — not yet implemented here.
RnsDestHash rnsNameHash(const std::string& appName, const std::vector<std::string>& aspects);

} // namespace omokoda
