// omokoda-mesh core loop.
//
// This file must stay board-agnostic. Board/radio differences are
// resolved entirely through the RadioHAL implementation selected at
// build time (see platformio.ini envs) — never with #ifdef branches
// in this file. If you find yourself wanting to branch here on
// OMOKODA_BOARD, that logic belongs in the HAL instead.

#include <Arduino.h>
#include "hal/radio_hal.h"
#include "packet/envelope.h"
#include "nostr_event.h"
#include "nostr/secp256k1_signer.h"

// Selected by the linker based on the active PlatformIO environment's
// lib_deps / build_flags — see lib/hal_lora_*/ for implementations.
extern omokoda::RadioHAL& getActiveRadioHAL();

static omokoda::RadioHAL* radio = nullptr;
static omokoda::NvsSecp256k1KeySource keySource;
static omokoda::Secp256k1NostrSigner signer(keySource);
static bool signerReady = false;

void setup() {
    Serial.begin(115200);
    Serial.printf("omokoda-mesh %s booting on %s\n",
                   OMOKODA_MESH_VERSION, OMOKODA_BOARD);

    radio = &getActiveRadioHAL();

    omokoda::RadioConfig cfg{};
    cfg.frequencyHz = 915000000; // TODO: region config (868/915/etc)
    cfg.spreadingFactor = 9;
    cfg.bandwidthKhz = 125;
    cfg.codingRate = 5;
    cfg.txPowerDbm = 17;

    if (!radio->begin(cfg)) {
        Serial.println("radio init failed");
    }

    // Dev/standalone fallback key provisioning. A node wired to real
    // ecosystem-derived key material (BIPON39/NIP-06) should supply its
    // own Secp256k1KeySource instead of NvsSecp256k1KeySource and skip
    // this call entirely.
    keySource.provisionIfAbsent();

    signerReady = signer.begin();
    if (signerReady) {
        Serial.printf("node npub (hex): %s\n", signer.pubkeyHex().c_str());
    } else {
        Serial.println("signer init failed — no key material or secp256k1 init error");
    }
}

void loop() {
    uint8_t buf[256];
    size_t n = radio->receive(buf, sizeof(buf));
    if (n > 0) {
        omokoda::Envelope env;
        if (omokoda::Envelope::deserialize(buf, n, env) && signerReady) {
            omokoda::NostrEvent ev = omokoda::envelopeToNostrEvent(env, signer.pubkeyHex());
            if (omokoda::nostrFinalizeEvent(ev, signer)) {
                // TODO: hand off to a gateway/relay transport instead of
                // just logging — this proves the sign path works
                // end-to-end but doesn't publish anywhere yet.
                Serial.println(omokoda::nostrEventToJson(ev).c_str());
            }
            // TODO: append hop signature, route to bridge(s), forward.
        }
    }
}
