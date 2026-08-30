#include "nostr/master_seed.h"

#if defined(ARDUINO) && defined(ESP32)
#include <Arduino.h>
#include <Preferences.h>
#define OMOKODA_HAVE_NVS 1
#endif

namespace omokoda {

namespace {
// 4-byte magic header a provisioning host sends before the 64 seed
// bytes, so a node doesn't mistake ordinary boot-time serial noise
// (garbage bytes from a host terminal opening the port, etc) for a seed.
// Not a security boundary — physical serial access already implies
// trust for Tier 2 (see docs/SEED_PROVISIONING.md "Explicitly not solved
// here") — this is purely a framing/sync marker.
constexpr uint8_t kSeedMagic[4] = {'O', 'M', 'S', 'D'};
} // namespace

bool NvsMasterSeedSource::getSeed(uint8_t out[64]) {
#if defined(OMOKODA_HAVE_NVS)
    Preferences prefs;
    if (!prefs.begin("omokoda_seed", /*readOnly=*/true)) return false;
    size_t n = prefs.getBytes("seed64", out, 64);
    prefs.end();
    return n == 64;
#else
    (void)out;
    return false;
#endif
}

bool NvsMasterSeedSource::provisionIfAbsent(const uint8_t seed[64]) {
#if defined(OMOKODA_HAVE_NVS)
    uint8_t existing[64];
    if (getSeed(existing)) {
        return false; // already provisioned, don't overwrite
    }

    Preferences prefs;
    if (!prefs.begin("omokoda_seed", /*readOnly=*/false)) return false;
    size_t written = prefs.putBytes("seed64", seed, 64);
    prefs.end();

    return written == 64;
#else
    (void)seed;
    return false;
#endif
}

bool SerialSeedProvisioner::provisionFromStream(Stream& stream, uint32_t timeoutMs,
                                                  NvsMasterSeedSource& store) {
#if defined(ARDUINO)
    uint8_t existing[64];
    if (store.getSeed(existing)) {
        return false; // never overwrite — caller should have checked
                       // this already, but don't trust that
    }

    uint32_t deadline = millis() + timeoutMs;
    size_t magicMatched = 0;

    // Wait for the 4-byte magic header, byte by byte, resetting the
    // match count on any mismatch (simple stream sync — tolerant of
    // leading garbage bytes on the line, matches only requires the
    // magic to appear contiguously once found).
    while (magicMatched < sizeof(kSeedMagic)) {
        if (millis() > deadline) return false;
        if (!stream.available()) continue;

        int b = stream.read();
        if (b == kSeedMagic[magicMatched]) {
            magicMatched++;
        } else {
            magicMatched = (b == kSeedMagic[0]) ? 1 : 0;
        }
    }

    // Read exactly 64 seed bytes.
    uint8_t seed[64];
    size_t received = 0;
    while (received < sizeof(seed)) {
        if (millis() > deadline) return false;
        if (!stream.available()) continue;

        int b = stream.read();
        if (b < 0) continue;
        seed[received++] = static_cast<uint8_t>(b);
    }

    bool ok = store.provisionIfAbsent(seed);

    // Best-effort scrub of the local copy.
    for (auto& byte : seed) byte = 0;

    return ok;
#else
    (void)stream;
    (void)timeoutMs;
    (void)store;
    return false;
#endif
}

} // namespace omokoda
