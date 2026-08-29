#pragma once
#include "nostr_event.h" // omokoda::NostrSigner
#include <cstdint>

// Real NostrSigner implementation: BIP-340 Schnorr signing over secp256k1
// via the vendored libsecp256k1 (lib/crypto_secp256k1). This is the
// concrete class the interface in lib/nostr_bridge/nostr_event.h was left
// abstract for.
//
// Deliberately out of scope here: deriving the private key from a
// BIPON39/NIP-06 mnemonic. That derivation is ecosystem-wide tooling
// (Omo-Koda2 wallet path) and shouldn't be duplicated in firmware. This
// signer instead takes the raw 32-byte secp256k1 private key from a
// pluggable Secp256k1KeySource — plug in a source that already holds
// ecosystem-derived key material, or the provided NVS-backed fallback for
// standalone/dev use.

namespace omokoda {

class Secp256k1KeySource {
public:
    virtual ~Secp256k1KeySource() = default;
    // Fills out[32] with the raw secp256k1 private key. Returns false on
    // failure (no key provisioned, storage error, etc).
    virtual bool getPrivateKey(uint8_t out[32]) = 0;
};

// NVS-backed key source for ESP32 (Arduino Preferences API). Stores/loads
// a raw 32-byte blob under namespace "omokoda_key" / key "seckey". No
// secure element — this is the "software isolation + best practices"
// fallback the portable-device spec calls for when no secure element is
// present, not a substitute for one. If a board with a secure element is
// targeted later, implement Secp256k1KeySource against that instead and
// leave this class alone.
class NvsSecp256k1KeySource : public Secp256k1KeySource {
public:
    bool getPrivateKey(uint8_t out[32]) override;

    // Provisions a new random key if none exists yet. Returns false if a
    // key was already present (does not overwrite) or on storage error.
    bool provisionIfAbsent();
};

class Secp256k1NostrSigner : public NostrSigner {
public:
    explicit Secp256k1NostrSigner(Secp256k1KeySource& keySource);
    ~Secp256k1NostrSigner() override;

    // Returns false if the underlying secp256k1 context or key failed to
    // initialize. Must be checked before use — pubkeyHex()/signId() are
    // undefined if init failed.
    bool begin();

    std::string pubkeyHex() const override;
    std::string signId(const std::string& idHex) override;

private:
    Secp256k1KeySource& _keySource;
    void* _ctx = nullptr;         // secp256k1_context*, opaque here to
                                   // keep libsecp256k1 headers out of this
                                   // public header
    uint8_t _seckey[32] = {0};
    uint8_t _xonlyPubkey[32] = {0};
    bool _ready = false;
};

} // namespace omokoda
