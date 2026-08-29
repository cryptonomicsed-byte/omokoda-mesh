#pragma once
#include <string>
#include <vector>
#include "packet/envelope.h"

// Converts between omokoda::Envelope and a signed Nostr event (NIP-01),
// using the kind 20000-20005 scheme + tags documented in
// docs/EVENT_KINDS.md. This is the only place Envelope <-> Nostr JSON
// conversion happens; radio/HAL code never sees Nostr JSON.

namespace omokoda {

struct NostrEvent {
    std::string id;         // 32-byte sha256, hex — computed, not set by caller
    std::string pubkeyHex;  // 32-byte x-only secp256k1 pubkey, hex
    uint32_t createdAt;
    uint16_t kind;
    std::vector<std::pair<std::string, std::vector<std::string>>> tags; // [name, [values...]]
    std::string content;
    std::string sigHex;     // 64-byte schnorr signature, hex — set by Signer
};

// Signs Nostr events. The real implementation lives outside this repo and
// wraps whatever key material the node already holds (BIPON39/NIP-06
// derived secp256k1 key — same class of key used elsewhere in the
// ecosystem, see docs/EVENT_KINDS.md "Open items"). This repo only defines
// the interface so the bridge logic can be built and tested against a
// fake/test signer before real key material is wired in.
class NostrSigner {
public:
    virtual ~NostrSigner() = default;
    virtual std::string pubkeyHex() const = 0;
    // Signs the 32-byte event id (already sha256'd per NIP-01) and returns
    // a 64-byte hex-encoded schnorr signature.
    virtual std::string signId(const std::string& idHex) = 0;
};

// Serializes the NIP-01 canonical form used for id computation:
// [0, pubkey, created_at, kind, tags, content] with compact JSON (no
// insignificant whitespace). Returns the UTF-8 byte string to be sha256'd.
std::string nostrCanonicalSerialize(const NostrEvent& ev);

// Computes and fills ev.id (sha256 of nostrCanonicalSerialize), then asks
// signer to produce ev.sig. Returns false if signing fails.
bool nostrFinalizeEvent(NostrEvent& ev, NostrSigner& signer);

// Envelope -> unsigned NostrEvent (kind + tags per docs/EVENT_KINDS.md).
// Caller still must call nostrFinalizeEvent() before publishing.
NostrEvent envelopeToNostrEvent(const Envelope& env, const std::string& pubkeyHex);

// Nostr event (already verified by caller) -> Envelope. Returns false if
// required tags (network, hop, origin_id) are missing/malformed.
bool nostrEventToEnvelope(const NostrEvent& ev, Envelope& out);

// Full JSON serialization of a finalized event, ready to publish to a relay.
std::string nostrEventToJson(const NostrEvent& ev);

} // namespace omokoda
