#pragma once
#include "packet/envelope.h"
#include "reticulum/rns_packet.h"

// Converts between omokoda::Envelope and RnsPacket, per the "Reticulum is
// an optional bridge, not core" decision (docs/ARCHITECTURE.md §2). A
// Reticulum destination hash is envelope metadata (originId), never an
// identity — the envelope's originPubkeyHex always carries the Nostr
// identity, regardless of which transport a packet arrived on.

namespace omokoda {

// RnsPacket -> Envelope. hexDestHash is the destHash rendered as lowercase
// hex for originId (matches the "reticulum_dest" Nostr tag format used in
// lib/nostr_bridge). Returns false if the packet has no payload to carry.
bool rnsPacketToEnvelope(const RnsPacket& pkt, Envelope& out);

// Envelope -> RnsPacket. Requires env.originId to be a 32-char hex string
// (16-byte destination hash) when env.originNetwork == Reticulum; for any
// other origin network, a destination hash is derived via rnsNameHash()
// using the given appName/aspects so the packet can still be routed onto
// a Reticulum interface as a bridge hop.
bool envelopeToRnsPacket(const Envelope& env, const std::string& bridgeAppName,
                          const std::vector<std::string>& bridgeAspects,
                          RnsPacket& out);

} // namespace omokoda
