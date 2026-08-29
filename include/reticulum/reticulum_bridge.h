#pragma once
#include "packet/envelope.h"
#include <Destination.h>
#include <Identity.h>
#include <Packet.h>

// Converts between omokoda::Envelope and real Reticulum types from the
// vendored microReticulum library (vendor/microReticulum, pulled into the
// build via platformio.ini lib_deps — the submodule is a pinned reference
// copy, not what actually compiles). Supersedes an earlier hand-rolled
// RnsPacket struct that guessed at RNS wire-format bit-packing; that guess
// is gone, this bridges to the real implementation instead. See
// docs/RETICULUM_BRIDGE.md for what changed and why.
//
// Per docs/ARCHITECTURE.md §2: Reticulum is an optional bridge, not core.
// A Reticulum destination hash is envelope metadata (originId), never an
// identity — Nostr (via Secp256k1NostrSigner) remains the sole root of
// trust. RNS::Identity below is a genuinely separate cryptographic domain
// (Ed25519/X25519, not secp256k1) required to speak real RNS at the
// transport layer — it cannot literally be "the same key" as the Nostr
// identity, only derived from the same root secret once the BIP-32
// derivation task (docs/EVENT_KINDS.md "Key derivation") lands.

namespace omokoda {

// RNS::Packet -> Envelope. Extracts the destination hash (hex) as
// originId, the raw packet payload (data), and hop count. The RNS
// transport-level Identity that signed/encrypted this packet (if any) is
// a separate concern from the envelope — see reticulumAttachIdentity().
bool rnsPacketToEnvelope(const RNS::Packet& pkt, Envelope& out);

// Envelope -> RNS::Packet, addressed to the given destination. Caller
// supplies the destination (constructed via RNS::Destination with the
// node's RNS::Identity and an app_name/aspects pair, or resolved from
// env.originId when env.originNetwork == Reticulum) rather than this
// function inventing one, since real destination construction needs a
// live RNS::Identity and interface context this bridge doesn't own.
RNS::Packet envelopeToRnsPacket(const Envelope& env, const RNS::Destination& destination);

// Derives an RNS destination hash the same way RNS itself does
// (RNS::Destination::name_hash), for the "bridging a non-Reticulum
// envelope onto a Reticulum interface" case — mirrors what the removed
// hand-rolled rnsNameHash() used to do, now backed by the real
// implementation instead of our own SHA256 truncation.
RNS::Bytes reticulumNameHash(const char* appName, const char* aspects);

} // namespace omokoda
