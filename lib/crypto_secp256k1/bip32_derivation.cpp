#include "nostr/bip32_derivation.h"
#include <secp256k1.h>
#include <SHA512.h>
#include <cstring>

namespace omokoda {

namespace {

constexpr char kBip32MasterHmacKey[] = "Bitcoin seed"; // matches BIPON39
                                                        // DerivationMode::Bip32,
                                                        // NOT its Native mode
                                                        // ("BIPỌ̀N39 master") —
                                                        // Native mode is a
                                                        // different derivation
                                                        // scheme, deliberately
                                                        // not used here.

void hmacSha512(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t dataLen,
                 uint8_t out64[64]) {
    SHA512 sha;
    sha.resetHMAC(key, keyLen);
    sha.update(data, dataLen);
    sha.finalizeHMAC(key, keyLen, out64, 64);
}

} // namespace

bool bip32MasterFromSeed(const uint8_t* seed, size_t seedLen, Bip32Key& out) {
    if (seedLen < 64) return false; // BIP-39 seeds are always 64 bytes;
                                     // reject shorter input rather than
                                     // silently deriving from too little
                                     // entropy.

    uint8_t digest[64];
    hmacSha512(reinterpret_cast<const uint8_t*>(kBip32MasterHmacKey),
               sizeof(kBip32MasterHmacKey) - 1, seed, seedLen, digest);

    std::memcpy(out.key, digest, 32);
    std::memcpy(out.chainCode, digest + 32, 32);
    return true;
}

bool bip32DeriveHardenedChild(const Bip32Key& parent, uint32_t index, Bip32Key& out) {
    uint32_t hardenedIndex = index | 0x80000000u;

    // HMAC input: 0x00 || parent_privkey(32) || index_be32(4) — 37 bytes.
    uint8_t input[37];
    input[0] = 0x00;
    std::memcpy(input + 1, parent.key, 32);
    input[33] = static_cast<uint8_t>((hardenedIndex >> 24) & 0xFF);
    input[34] = static_cast<uint8_t>((hardenedIndex >> 16) & 0xFF);
    input[35] = static_cast<uint8_t>((hardenedIndex >> 8) & 0xFF);
    input[36] = static_cast<uint8_t>(hardenedIndex & 0xFF);

    uint8_t digest[64];
    hmacSha512(parent.chainCode, 32, input, sizeof(input), digest);

    // child_key = (IL + parent_key) mod n. secp256k1_ec_seckey_tweak_add
    // does exactly this (seckey += tweak mod n) and itself rejects the
    // astronomically-rare invalid-scalar / zero-result cases per BIP-32,
    // matching BIPON39's add_mod_n() semantics without reimplementing
    // scalar arithmetic by hand.
    uint8_t childKey[32];
    std::memcpy(childKey, parent.key, 32);

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    int ok = secp256k1_ec_seckey_tweak_add(ctx, childKey, digest /* IL, first 32 bytes */);
    secp256k1_context_destroy(ctx);

    if (!ok) return false;

    std::memcpy(out.key, childKey, 32);
    std::memcpy(out.chainCode, digest + 32, 32);
    return true;
}

bool bip32DeriveHardenedPath(const uint8_t* seed, size_t seedLen,
                               const uint32_t* indices, size_t indexCount,
                               Bip32Key& out) {
    Bip32Key current;
    if (!bip32MasterFromSeed(seed, seedLen, current)) return false;

    for (size_t i = 0; i < indexCount; ++i) {
        Bip32Key next;
        if (!bip32DeriveHardenedChild(current, indices[i], next)) return false;
        current = next;
    }

    out = current;
    return true;
}

} // namespace omokoda
