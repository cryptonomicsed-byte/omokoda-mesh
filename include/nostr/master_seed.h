#pragma once
#include <cstdint>
#include <cstddef>

// Forward-declared at global scope, matching Arduino's own ::Stream
// (defined in Arduino.h, base class of Serial/HardwareSerial). Declared
// here rather than inside namespace omokoda so the elaborated-type-specifier
// below unambiguously refers to the real global Stream, not an
// accidentally-introduced omokoda::Stream.
class Stream;

// Sources for a node's 64-byte BIP-32 master seed — the root
// DerivedSecp256k1KeySource and deriveRnsIdentity() both derive from. See
// docs/SEED_PROVISIONING.md for the full design (three tiers matched to
// real hardware classes) and why this needed new design rather than
// reusing an existing ecosystem flow verbatim.

namespace omokoda {

class MasterSeedSource {
public:
    virtual ~MasterSeedSource() = default;
    // Fills out[64] with the master seed. Returns false if none is
    // provisioned yet or on storage error.
    virtual bool getSeed(uint8_t out[64]) = 0;
};

// Persisted seed storage (ESP32 NVS via Arduino Preferences), Tier 2/3 of
// docs/SEED_PROVISIONING.md. Write-once semantics matching
// NvsSecp256k1KeySource::provisionIfAbsent() — see
// SerialSeedProvisioner below for how the seed actually gets in here on a
// headless node.
class NvsMasterSeedSource : public MasterSeedSource {
public:
    bool getSeed(uint8_t out[64]) override;

    // Persists a seed if none exists yet. Returns false if a seed was
    // already present (does not overwrite) or on storage error. Exposed
    // separately from getSeed() so provisioning tools (like
    // SerialSeedProvisioner) can write without every caller needing
    // write access.
    bool provisionIfAbsent(const uint8_t seed[64]);
};

// Tier 2 of docs/SEED_PROVISIONING.md: one-shot seed injection over
// serial from a trusted provisioning host (a laptop, or a gateway node
// that already completed its own birth sequence). Protocol: waits for a
// 4-byte magic header (see kSeedMagic in the .cpp), then reads exactly 64
// bytes within a timeout. Never overwrites an existing seed — call
// NvsMasterSeedSource::getSeed() first and only invoke this if it fails.
//
// Any Stream-like transport (Serial today; BLE/QR-relayed bytes later)
// can carry the same protocol — see docs/SEED_PROVISIONING.md "Tier 2".
class SerialSeedProvisioner {
public:
    // Blocks up to timeoutMs waiting for the magic header + 64 seed
    // bytes on the given stream. Returns true and writes to
    // NvsMasterSeedSource on success. Caller should only call this in an
    // explicit provisioning mode (e.g. no seed yet + a button held at
    // boot), not on every normal boot.
    static bool provisionFromStream(::Stream& stream, uint32_t timeoutMs,
                                      NvsMasterSeedSource& store);
};

} // namespace omokoda
