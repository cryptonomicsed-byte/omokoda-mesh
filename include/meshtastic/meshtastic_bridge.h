#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "packet/envelope.h"

// Converts between omokoda::Envelope and a Meshtastic MeshPacket.
//
// Field numbers are from meshtastic/protobufs `mesh.proto`, read directly
// (2026-09-04) rather than from memory — the same custom-fit rule this repo
// applies to ip-layer and BIPON39. Only the fields the bridge needs are
// handled; every other field is skipped by wire type, so a newer Meshtastic
// firmware adding fields does not break decoding.
//
// **Licence note.** docs/MESHTASTIC_ANDROID_PATTERNS.md records that
// Meshtastic-Android is GPLv3 and that no code was copied from it. Nothing is
// copied here either: protobuf field numbers are the wire interface, and this
// is an independent encoder/decoder for the fields the bridge maps. No
// Meshtastic source is vendored, generated from, or linked.
//
// **What does not map.** A Meshtastic frame is not signed, and its `from` is
// a 32-bit node number assigned by the device, not a key. So a decoded frame
// yields an Envelope whose originPubkeyHex and hopTrail are both empty. A
// gateway signs the resulting kind-20000 event with its own key and must not
// present the node number as an identity.

namespace omokoda {

// MeshPacket, mesh.proto
constexpr uint32_t kMeshtasticFieldFrom      = 1;   // fixed32
constexpr uint32_t kMeshtasticFieldTo        = 2;   // fixed32
constexpr uint32_t kMeshtasticFieldChannel   = 3;   // uint32
constexpr uint32_t kMeshtasticFieldDecoded   = 4;   // Data
constexpr uint32_t kMeshtasticFieldEncrypted = 5;   // bytes
constexpr uint32_t kMeshtasticFieldId        = 6;   // fixed32
constexpr uint32_t kMeshtasticFieldRxTime    = 7;   // fixed32
constexpr uint32_t kMeshtasticFieldHopLimit  = 9;   // uint32
constexpr uint32_t kMeshtasticFieldViaMqtt   = 14;  // bool
constexpr uint32_t kMeshtasticFieldHopStart  = 15;  // uint32
constexpr uint32_t kMeshtasticFieldRelayNode = 19;  // uint32

// Data, mesh.proto
constexpr uint32_t kMeshtasticDataPortnum = 1;  // PortNum enum
constexpr uint32_t kMeshtasticDataPayload = 2;  // bytes

// The protocol's own ceiling. A frame claiming more is malformed rather than
// well-travelled.
constexpr uint8_t kMeshtasticMaxHopLimit = 7;

struct MeshtasticFrame {
    uint32_t packetId = 0;
    uint32_t fromNode = 0;
    uint32_t toNode = 0;
    uint32_t channel = 0;
    uint32_t rxTime = 0;
    uint8_t hopLimit = 0;
    uint8_t hopStart = 0;
    uint32_t relayNode = 0;
    bool viaMqtt = false;
    uint32_t portnum = 0;
    std::vector<uint8_t> payload;
    // True when the payload_variant arm was `encrypted`. There is nothing to
    // decode without the channel key, and this bridge holds none.
    bool encrypted = false;
};

// Decodes a MeshPacket. Returns false on a truncated or malformed frame, or
// one missing the fields a real received packet always carries — protobuf has
// no terminator, so a message cut exactly on a field boundary decodes cleanly
// as a shorter one and only a required-field check catches it.
bool meshtasticDecode(const uint8_t* data, size_t len, MeshtasticFrame& out);

// Encodes a MeshPacket. Round-trips with meshtasticDecode for the fields
// above; fields this bridge does not model are not emitted.
std::vector<uint8_t> meshtasticEncode(const MeshtasticFrame& frame);

// Hops actually taken: hopStart - hopLimit, or -1 where hopStart is unset.
//
// -1 rather than 0, and the distinction is load-bearing: firmware older than
// hop_start does not send one, and reporting 0 for those would claim every
// such packet arrived direct.
int meshtasticHopsTaken(const MeshtasticFrame& frame);

// The `!hex` form Meshtastic shows users, and what belongs in an Envelope's
// originId so the origin_id tag matches what an operator sees on their device.
std::string meshtasticNodeId(uint32_t nodeNumber);

// MeshtasticFrame -> Envelope, ready for the Nostr bridge or the on-air form.
//
// originId is the *packet* id, which is what identifies this frame in its
// native network; the sending node goes nowhere near originPubkeyHex, because
// it is a device-assigned number and not a key.
Envelope meshtasticToEnvelope(const MeshtasticFrame& frame);

// Envelope -> MeshtasticFrame. Returns false where the envelope did not come
// from Meshtastic, since the node numbers a frame needs cannot be invented
// from another network's addressing.
//
// `fromNode` is left at 0: an Envelope carries no Meshtastic node number (see
// meshtasticToEnvelope), so the only correct value is the transmitting node's
// own, which this function cannot know. Set it before transmitting.
// meshtasticDecode rejects a frame still holding 0, so forgetting fails
// loudly rather than putting an unaddressed packet on the air.
bool envelopeToMeshtastic(const Envelope& env, MeshtasticFrame& out);

}  // namespace omokoda
