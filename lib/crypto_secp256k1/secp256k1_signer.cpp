#include "nostr/secp256k1_signer.h"
#include "nostr/bip32_derivation.h"
#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_schnorrsig.h>
#include <cstring>
#include <cstdio>

#if defined(ARDUINO) && defined(ESP32)
#include <Preferences.h>
#include <esp_system.h>
#define OMOKODA_HAVE_ESP_RANDOM 1
#endif

namespace omokoda {

namespace {

std::string bytesToHex(const uint8_t* data, size_t len) {
    static const char* hexChars = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(hexChars[(data[i] >> 4) & 0xF]);
        out.push_back(hexChars[data[i] & 0xF]);
    }
    return out;
}

bool hexToBytes(const std::string& hex, uint8_t* out, size_t outLen) {
    if (hex.size() != outLen * 2) return false;
    for (size_t i = 0; i < outLen; ++i) {
        unsigned int byte;
        if (sscanf(hex.c_str() + i * 2, "%2x", &byte) != 1) return false;
        out[i] = static_cast<uint8_t>(byte);
    }
    return true;
}

void fillRandom(uint8_t* buf, size_t len) {
#if defined(OMOKODA_HAVE_ESP_RANDOM)
    esp_fill_random(buf, len);
#else
    // Non-ESP32 build (host unit tests, etc): NOT cryptographically
    // secure. Real firmware builds must go through the ESP32 branch
    // above, which uses the hardware RNG. This fallback exists only so
    // the signer compiles for host-side tests of the signing logic
    // itself, not for anything that touches real key material.
    for (size_t i = 0; i < len; ++i) buf[i] = static_cast<uint8_t>(rand());
#endif
}

} // namespace

// ---- NvsSecp256k1KeySource ----

bool NvsSecp256k1KeySource::getPrivateKey(uint8_t out[32]) {
#if defined(ARDUINO) && defined(ESP32)
    Preferences prefs;
    if (!prefs.begin("omokoda_key", /*readOnly=*/true)) return false;
    size_t n = prefs.getBytes("seckey", out, 32);
    prefs.end();
    return n == 32;
#else
    (void)out;
    return false;
#endif
}

bool NvsSecp256k1KeySource::provisionIfAbsent() {
#if defined(ARDUINO) && defined(ESP32)
    uint8_t existing[32];
    if (getPrivateKey(existing)) {
        return false; // already provisioned, don't overwrite
    }

    // Reject the all-zero key on the (astronomically unlikely) chance
    // fillRandom ever produced it, per secp256k1 validity rules.
    uint8_t candidate[32];
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    bool valid = false;
    for (int attempt = 0; attempt < 8 && !valid; ++attempt) {
        fillRandom(candidate, 32);
        valid = secp256k1_ec_seckey_verify(ctx, candidate) == 1;
    }
    secp256k1_context_destroy(ctx);
    if (!valid) return false;

    Preferences prefs;
    if (!prefs.begin("omokoda_key", /*readOnly=*/false)) return false;
    size_t written = prefs.putBytes("seckey", candidate, 32);
    prefs.end();

    return written == 32;
#else
    return false;
#endif
}

// ---- DerivedSecp256k1KeySource ----

DerivedSecp256k1KeySource::DerivedSecp256k1KeySource(const uint8_t seed[64], uint32_t nodeIndex)
    : _nodeIndex(nodeIndex) {
    std::memcpy(_seed, seed, 64);
}

bool DerivedSecp256k1KeySource::getPrivateKey(uint8_t out[32]) {
    // m/44'/20000'/<node_index>'/0' — all hardened, matching minipae's
    // own m/44'/<kind>'/agent'/owner' convention. 20000 is omokoda-mesh's
    // primary kind (Mesh Packet Route), reused as the BIP-32 purpose
    // segment the same way minipae reuses its own kind 30174. See
    // docs/EVENT_KINDS.md "Key derivation".
    const uint32_t path[] = {44, 20000, _nodeIndex, 0};

    Bip32Key derived;
    if (!bip32DeriveHardenedPath(_seed, sizeof(_seed), path, 4, derived)) {
        return false;
    }

    std::memcpy(out, derived.key, 32);
    std::memset(derived.key, 0, sizeof(derived.key)); // best-effort scrub
    return true;
}

// ---- Secp256k1NostrSigner ----

Secp256k1NostrSigner::Secp256k1NostrSigner(Secp256k1KeySource& keySource)
    : _keySource(keySource) {}

Secp256k1NostrSigner::~Secp256k1NostrSigner() {
    if (_ctx) {
        secp256k1_context_destroy(static_cast<secp256k1_context*>(_ctx));
    }
    // Best-effort scrub of the copy we hold. Not a substitute for real
    // secure-element storage — see class docs in secp256k1_signer.h.
    std::memset(_seckey, 0, sizeof(_seckey));
}

bool Secp256k1NostrSigner::begin() {
    if (!_keySource.getPrivateKey(_seckey)) {
        return false;
    }

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if (!ctx) return false;

    uint8_t seed[32];
    fillRandom(seed, sizeof(seed));
    if (!secp256k1_context_randomize(ctx, seed)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    if (!secp256k1_ec_seckey_verify(ctx, _seckey)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(ctx, &keypair, _seckey)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    secp256k1_xonly_pubkey xonly;
    if (!secp256k1_keypair_xonly_pub(ctx, &xonly, nullptr, &keypair)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    if (!secp256k1_xonly_pubkey_serialize(ctx, _xonlyPubkey, &xonly)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    _ctx = ctx;
    _ready = true;
    return true;
}

std::string Secp256k1NostrSigner::pubkeyHex() const {
    if (!_ready) return "";
    return bytesToHex(_xonlyPubkey, sizeof(_xonlyPubkey));
}

std::string Secp256k1NostrSigner::signId(const std::string& idHex) {
    if (!_ready) return "";

    uint8_t msg32[32];
    if (!hexToBytes(idHex, msg32, sizeof(msg32))) return "";

    auto* ctx = static_cast<secp256k1_context*>(_ctx);

    secp256k1_keypair keypair;
    if (!secp256k1_keypair_create(ctx, &keypair, _seckey)) return "";

    uint8_t auxRand[32];
    fillRandom(auxRand, sizeof(auxRand));

    uint8_t sig64[64];
    if (!secp256k1_schnorrsig_sign32(ctx, sig64, msg32, &keypair, auxRand)) {
        return "";
    }

    return bytesToHex(sig64, sizeof(sig64));
}

} // namespace omokoda
