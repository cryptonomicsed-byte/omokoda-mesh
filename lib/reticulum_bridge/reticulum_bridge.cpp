#include "reticulum/reticulum_bridge.h"
#include "nostr/bip32_derivation.h"
#include <cstring>

namespace omokoda {

bool rnsPacketToEnvelope(const RNS::Packet& pkt, Envelope& out) {
    out.originNetwork = SourceNetwork::Reticulum;
    out.originId = pkt.destination_hash().toHex();
    out.originPubkeyHex.clear(); // RNS transport identity is a separate
                                  // crypto domain from the Nostr identity
                                  // (see reticulum_bridge.h) — caller
                                  // attaches a Nostr pubkey from context
                                  // if this packet is being re-emitted as
                                  // a kind-20000 event.
    out.createdAt = 0; // RNS packets carry no timestamp at this layer;
                        // caller should stamp with local receive time.
    out.hopCount = pkt.hops();
    out.nostrKind = 20000; // Mesh Packet Route by default

    const RNS::Bytes& data = pkt.data();
    out.payload.assign(data.data(), data.data() + data.size());

    return true;
}

RNS::Packet envelopeToRnsPacket(const Envelope& env, const RNS::Destination& destination) {
    RNS::Bytes payload(env.payload.data(), env.payload.size());
    RNS::Packet pkt(destination, payload);
    pkt.hops(env.hopCount);
    return pkt;
}

RNS::Bytes reticulumNameHash(const char* appName, const char* aspects) {
    return RNS::Destination::name_hash(appName, aspects);
}

bool deriveRnsIdentity(const uint8_t seed[64], uint32_t nodeIndex, RNS::Identity& out) {
    // m/44'/20000'/<node_index>'/1' -> raw material for the X25519
    // (encryption) half; .../2' -> raw material for the Ed25519
    // (signing) half. Both are ordinary BIP-32 hardened secp256k1
    // children — using their 32-byte scalar output as generic key-seed
    // entropy for a different curve is the standard multi-curve KDF
    // pattern (X25519/Ed25519 clamp/expand their input internally
    // regardless of which curve produced it), not a claim that the
    // curves are somehow unified.
    const uint32_t x25519Path[] = {44, 20000, nodeIndex, 1};
    const uint32_t ed25519Path[] = {44, 20000, nodeIndex, 2};

    Bip32Key x25519Half;
    Bip32Key ed25519Half;
    if (!bip32DeriveHardenedPath(seed, 64, x25519Path, 4, x25519Half)) return false;
    if (!bip32DeriveHardenedPath(seed, 64, ed25519Path, 4, ed25519Half)) return false;

    // RNS::Identity::load_private_key() expects exactly this layout:
    // first KEYSIZE/8/2 (32) bytes = X25519 private key, remaining 32
    // bytes = Ed25519 private key seed — verified against
    // vendor/microReticulum/src/Identity.cpp's load_private_key()
    // directly, not assumed from the general RNS spec description.
    uint8_t prvBytes[64];
    std::memcpy(prvBytes, x25519Half.key, 32);
    std::memcpy(prvBytes + 32, ed25519Half.key, 32);

    RNS::Identity identity(false); // create_keys=false: we're loading, not generating
    bool ok = identity.load_private_key(RNS::Bytes(prvBytes, sizeof(prvBytes)));

    std::memset(prvBytes, 0, sizeof(prvBytes)); // best-effort scrub

    if (!ok) return false;
    out = identity;
    return true;
}

} // namespace omokoda
