// omokoda-mesh core loop.
//
// This file must stay board-agnostic. Board/radio differences are
// resolved entirely through the RadioHAL implementation selected at
// build time (see platformio.ini envs) — never with #ifdef branches
// in this file. If you find yourself wanting to branch here on
// OMOKODA_BOARD, that logic belongs in the HAL instead.

#include <Arduino.h>
#include <cstring>
#include "hal/radio_hal.h"
#include "packet/envelope.h"
#include "nostr_event.h"
#include "nostr/secp256k1_signer.h"
#include "nostr/master_seed.h"
#include "reticulum/reticulum_bridge.h"

#ifndef OMOKODA_NODE_INDEX
// Per-node hardened index at m/44'/20000'/<OMOKODA_NODE_INDEX>'/{0,1,2}
// (see docs/EVENT_KINDS.md "Key derivation" and docs/RETICULUM_BRIDGE.md).
// Defaults to 0 — real fleets need a real per-node assignment scheme
// (not designed yet); override with -D OMOKODA_NODE_INDEX=<n> per board
// until one exists, so at least two nodes sharing a seed don't collide
// on index 0 by accident.
#define OMOKODA_NODE_INDEX 0
#endif

// Serial provisioning window: how long setup() waits for a
// SerialSeedProvisioner handshake before falling back to the random dev
// key. See docs/SEED_PROVISIONING.md Tier 2.
#define OMOKODA_SEED_PROVISION_TIMEOUT_MS 5000

// Selected by the linker based on the active PlatformIO environment's
// lib_deps / build_flags — see lib/hal_lora_*/ for implementations.
extern omokoda::RadioHAL& getActiveRadioHAL();

static omokoda::RadioHAL* radio = nullptr;
static omokoda::NvsMasterSeedSource seedSource;
static omokoda::NvsSecp256k1KeySource devKeySource; // Tier-2-absent fallback,
                                                     // see docs/SEED_PROVISIONING.md
static omokoda::Secp256k1KeySource* activeKeySource = nullptr;
static omokoda::Secp256k1NostrSigner* signer = nullptr;
// RNS::Identity constructed lazily inside setup(), not as a static global
// — RNS is microReticulum's own top-level namespace (not nested under
// omokoda, see include/reticulum/reticulum_bridge.h), and its constructor
// may depend on other RNS internal state that isn't guaranteed
// initialized yet during C++ static-init (before Arduino's setup() runs).
static RNS::Identity* rnsIdentity = nullptr;
static bool rnsIdentityReady = false;
static bool signerReady = false;

// Waits for a seed over Serial (docs/SEED_PROVISIONING.md Tier 2). Only
// called when no seed is provisioned yet — never overwrites one that
// already exists (SerialSeedProvisioner enforces this itself too).
static bool tryProvisionSeedFromSerial(uint8_t seed[64]) {
    Serial.println("no master seed provisioned — waiting for serial injection "
                    "(4-byte magic 'OMSD' + 64 bytes)...");
    if (!omokoda::SerialSeedProvisioner::provisionFromStream(
            Serial, OMOKODA_SEED_PROVISION_TIMEOUT_MS, seedSource)) {
        return false;
    }
    return seedSource.getSeed(seed);
}

void setup() {
    Serial.begin(115200);
    Serial.printf("omokoda-mesh %s booting on %s (node index %d)\n",
                   OMOKODA_MESH_VERSION, OMOKODA_BOARD, OMOKODA_NODE_INDEX);

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

    // Master-seed boot path (docs/SEED_PROVISIONING.md): use a real
    // derived identity if a seed is already provisioned or can be
    // provisioned now over serial; otherwise fall back to the random
    // dev/standalone key so the node still boots usable for testing.
    uint8_t seed[64];
    bool haveSeed = seedSource.getSeed(seed);
    if (!haveSeed) {
        haveSeed = tryProvisionSeedFromSerial(seed);
    }

    static omokoda::DerivedSecp256k1KeySource* derivedKeySource = nullptr;
    if (haveSeed) {
        derivedKeySource = new omokoda::DerivedSecp256k1KeySource(seed, OMOKODA_NODE_INDEX);
        activeKeySource = derivedKeySource;
        Serial.println("using real BIP-32-derived key (m/44'/20000'/"
                        + String(OMOKODA_NODE_INDEX) + "'/0')");

        rnsIdentity = new RNS::Identity(false);
        if (omokoda::deriveRnsIdentity(seed, OMOKODA_NODE_INDEX, *rnsIdentity)) {
            rnsIdentityReady = true;
            Serial.printf("RNS identity hash: %s\n", rnsIdentity->hexhash().c_str());
        } else {
            Serial.println("RNS identity derivation failed");
        }
    } else {
        Serial.println("no seed available — falling back to random dev/"
                        "standalone key (docs/SEED_PROVISIONING.md); this "
                        "node's identity will NOT match on next reflash/"
                        "reboot with a different random key unless NVS "
                        "persists");
        devKeySource.provisionIfAbsent();
        activeKeySource = &devKeySource;
    }

    // Best-effort scrub of the local seed copy — the derived key source
    // holds its own copy internally, this stack copy isn't needed again.
    memset(seed, 0, sizeof(seed));

    signer = new omokoda::Secp256k1NostrSigner(*activeKeySource);
    signerReady = signer->begin();
    if (signerReady) {
        Serial.printf("node npub (hex): %s\n", signer->pubkeyHex().c_str());
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
            omokoda::NostrEvent ev = omokoda::envelopeToNostrEvent(env, signer->pubkeyHex());
            if (omokoda::nostrFinalizeEvent(ev, *signer)) {
                // TODO: hand off to a gateway/relay transport instead of
                // just logging — this proves the sign path works
                // end-to-end but doesn't publish anywhere yet.
                Serial.println(omokoda::nostrEventToJson(ev).c_str());
            }
            // TODO: append hop signature, route to bridge(s), forward.
        }
    }
}
