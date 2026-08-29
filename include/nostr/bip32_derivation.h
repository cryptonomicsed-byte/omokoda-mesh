#pragma once
#include <cstdint>
#include <cstddef>

// BIP-32 hardened derivation over secp256k1, matching BIPON39's Bip32Mode
// exactly (cryptonomicsed-byte/BIPON39, src/derivation.rs): master key =
// HMAC-SHA512("Bitcoin seed", seed), child = (IL + parent) mod n via
// HMAC-SHA512(parent_chain_code, 0x00 || parent_key || index_be32) for
// hardened indices. This exists because BIPON39 itself is a std-only Rust
// crate with no no_std/FFI target — can't be linked into this firmware
// directly, so the same math is reimplemented here in C++ against the
// secp256k1 already vendored for Secp256k1NostrSigner (lib/crypto_secp256k1).
//
// Only hardened derivation is implemented — the one omokoda-mesh path
// (m/44'/20000'/<node_index>'/0', see docs/EVENT_KINDS.md "Key
// derivation") and minipae's own m/44'/30174'/agent'/owner' are both
// all-hardened. Non-hardened derivation (which needs the parent's
// compressed public key, not just its private key, per BIP-32) is
// deliberately not implemented since nothing in this ecosystem's key
// paths uses it yet.

namespace omokoda {

struct Bip32Key {
    uint8_t key[32];
    uint8_t chainCode[32];
};

// Derives the master key from a >=64-byte seed (e.g. a BIP-39 seed —
// producing that seed from a mnemonic is out of scope here, same
// boundary as Secp256k1KeySource: this firmware consumes key material,
// it doesn't own mnemonic/wordlist logic). Matches BIPON39's
// DerivationMode::Bip32 (HMAC key "Bitcoin seed"), not its Native mode.
bool bip32MasterFromSeed(const uint8_t* seed, size_t seedLen, Bip32Key& out);

// Derives one hardened child: index is the unhardened index (0..2^31-1),
// this function ORs in the hardened bit (0x80000000) itself — callers
// should not pre-OR it, to match the ergonomics of BIPON39's own
// path-array convention where each hardened element already includes the
// bit; this C++ version takes the plain index for clarity at each call
// site instead of requiring the caller to OR it manually every time.
bool bip32DeriveHardenedChild(const Bip32Key& parent, uint32_t index, Bip32Key& out);

// Derives a full path of hardened indices in one call, e.g.
// {44, 20000, nodeIndex, 0} for m/44'/20000'/<nodeIndex>'/0'.
bool bip32DeriveHardenedPath(const uint8_t* seed, size_t seedLen,
                               const uint32_t* indices, size_t indexCount,
                               Bip32Key& out);

} // namespace omokoda
