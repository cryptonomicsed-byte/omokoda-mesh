#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Cross-Protocol Packet Wrapper. Every frame this node ever handles —
// whether it originated as a Nostr event, a Reticulum packet, a
// Meshtastic protobuf, or a raw LoRaWAN uplink — gets canonicalized
// into this envelope before it touches routing, signing, or the
// proof-of-relay hop chain. Bridges translate INTO this shape on
// ingress and OUT of it on egress; nothing else in the firmware should
// need to know which network a packet came from.
//
// This envelope is the wire-compatible skeleton for the Nostr event
// kinds 20000-20005 described in docs/EVENT_KINDS.md — a Nostr bridge
// serializes an Envelope into a signed kind-2000x event; other bridges
// (Reticulum, Meshtastic) serialize it into their native formats.

namespace omokoda {

enum class SourceNetwork : uint8_t {
    Nostr = 0,
    Reticulum = 1,
    Meshtastic = 2,
    LoRaWAN = 3,
    DePIN = 4,
    Local = 5,
};

struct HopSignature {
    std::string relayPubkeyHex;   // secp256k1 or Ed25519 pubkey of the relaying node
    std::string signatureHex;
    uint32_t timestamp;
};

struct Envelope {
    SourceNetwork originNetwork;
    std::string originId;          // native ID/hash in the origin network (e.g. Reticulum dest hash)
    std::string originPubkeyHex;   // sender identity, network-native encoding preserved verbatim
    uint32_t createdAt;
    uint8_t hopCount = 0;
    std::vector<HopSignature> hopTrail;
    std::vector<uint8_t> payload;  // opaque; interpretation depends on originNetwork + kind

    // Nostr kind this envelope maps to when bridged onto Nostr (20000-20005).
    // See docs/EVENT_KINDS.md. 0 = unset/not yet classified.
    uint16_t nostrKind = 0;

    std::vector<uint8_t> serialize() const;
    static bool deserialize(const uint8_t* data, size_t len, Envelope& out);
};

} // namespace omokoda
