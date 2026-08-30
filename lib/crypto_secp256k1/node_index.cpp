#include "nostr/node_index.h"
#include <SHA256.h>
#include <cstring>

#if defined(ARDUINO) && defined(ESP32)
#include <Arduino.h>
#endif

namespace omokoda {

uint32_t deriveNodeIndexFromHardwareId() {
#if defined(ARDUINO) && defined(ESP32)
    uint64_t mac = ESP.getEfuseMac(); // factory-burned, globally unique, permanent

    uint8_t macBytes[8];
    for (int i = 0; i < 8; ++i) {
        macBytes[i] = static_cast<uint8_t>((mac >> (8 * i)) & 0xFF);
    }

    SHA256 sha;
    sha.update(macBytes, sizeof(macBytes));
    uint8_t digest[32];
    sha.finalize(digest, sizeof(digest));

    uint32_t index = (static_cast<uint32_t>(digest[0]) << 24) |
                      (static_cast<uint32_t>(digest[1]) << 16) |
                      (static_cast<uint32_t>(digest[2]) << 8) |
                       static_cast<uint32_t>(digest[3]);

    return index & 0x7FFFFFFFu; // stay under the hardened-index boundary;
                                 // bip32DeriveHardenedChild() ORs in 0x80000000 itself
#else
    return 0;
#endif
}

} // namespace omokoda
