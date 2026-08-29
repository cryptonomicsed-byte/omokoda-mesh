# omokoda-mesh

Universal, hardware-agnostic ESP32 LoRa mesh node firmware. Nostr-native
packet format, cross-network relay (Nostr / Reticulum / Meshtastic /
LoRaWAN / DePIN), proof-of-relay hop signatures.

Part of the Ọmọ Kọ́dà / OSOVM / Vantage ecosystem — this is a concrete
implementation of the still-unbuilt first step in the locked
`osovm-witness-attestation-architecture` design (Reticulum mesh for
identity/signing + Nostr/Bondhive pattern for economics/reputation + DePIN
transport hops). It is not a separate/competing identity or signing scheme.

## Status

- SX1262 radio HAL: **implemented** (`lib/hal_lora_sx1262`, via RadioLib).
- `Envelope` binary wire format: **implemented** (`lib/packet_wrapper`).
- Nostr bridge (`Envelope` <-> signed kind-2000x event, NIP-01 id/canonical
  JSON): **implemented** (`lib/nostr_bridge`).
- `NostrSigner`: **implemented for real** —
  `Secp256k1NostrSigner` (`lib/crypto_secp256k1`) does actual BIP-340
  Schnorr signing via a vendored libsecp256k1 build (`vendor/secp256k1-embedded`,
  EXTRAKEYS+SCHNORRSIG modules enabled). Key material comes from a
  pluggable `Secp256k1KeySource`; the default `NvsSecp256k1KeySource` is a
  dev/standalone fallback (raw key in ESP32 NVS, no secure element) — a
  node wired to real ecosystem BIPON39/NIP-06-derived keys should supply
  its own `Secp256k1KeySource` instead. See
  `include/nostr/secp256k1_signer.h`. `src/main.cpp` now signs and logs a
  full Nostr event end-to-end (not yet published anywhere — no relay
  transport wired in).
- Reticulum bridge (`Envelope` <-> RNS packet shape): **implemented**
  (`lib/reticulum_bridge`), packet-shape only — no RNS Identity
  encryption/signing, no transport routing. See
  `docs/RETICULUM_BRIDGE.md` for exact gaps before treating it as
  interop-tested against real Reticulum nodes.
- SX1276, CC1101, BLE, WiFi HALs and the Meshtastic bridge (superseded by
  the `omokoda-mesh-firmware` fork instead): **stubs, not yet
  implemented**.
- Untested on real hardware — no board has been flashed yet.

## Design principles

1. **One firmware core, many boards.** Board/radio differences live only in
   the HAL layer (`include/hal/radio_hal.h`, `lib/hal_*`). `src/main.cpp`
   never branches on board identity.
2. **One packet shape.** Every frame — regardless of origin network — is
   canonicalized into `omokoda::Envelope` (`include/packet/envelope.h`)
   before routing, signing, or hop-chain logic touches it. Bridges convert
   in/out of this shape; nothing else needs to know the origin network.
3. **Nostr is core identity; Reticulum is an optional bridge; Meshtastic
   interop is a real fork (`omokoda-mesh-firmware`), not a wrap.** See
   `docs/ARCHITECTURE.md` for the full layering decision, including the
   OSOVM (Julia sim validation + Gaussian splatting) tie-in for spatial
   telemetry.

## Layout

- `platformio.ini` — build envs for generic ESP32/S3/C3 + known community
  boards (Heltec V3, TTGO T-Beam, Cardputer ADV). Add a board by adding an
  env, not by forking source.
- `include/hal/` — radio abstraction interface.
- `include/packet/` — cross-protocol envelope.
- `lib/hal_lora_sx1262` — real SX1262 driver (RadioLib-backed).
- `lib/hal_lora_sx1276`, `lib/hal_lora_cc1101`, `lib/hal_ble`, `lib/hal_wifi`
  — stub HAL drivers, not yet implemented.
- `lib/packet_wrapper` — `Envelope` binary (de)serialization, implemented.
- `lib/nostr_bridge` — `Envelope` <-> Nostr event conversion + NIP-01
  id/canonical serialization, implemented.
- `lib/crypto_secp256k1` — real `NostrSigner` implementation
  (BIP-340 Schnorr via vendored libsecp256k1) + NVS dev keystore.
- `vendor/secp256k1-embedded` — git submodule, upstream libsecp256k1
  adapted for embedded targets; our own config header
  (`lib/crypto_secp256k1/libsecp256k1-config.h`) enables the
  EXTRAKEYS+SCHNORRSIG modules the wrapper doesn't turn on by default.
- `lib/reticulum_bridge` — `Envelope` <-> RNS packet-shape conversion,
  implemented (no RNS Identity crypto yet — see `docs/RETICULUM_BRIDGE.md`).
- `lib/meshtastic_bridge` — stub; superseded by the
  `omokoda-mesh-firmware` fork for real Meshtastic-mesh interop.
- `docs/EVENT_KINDS.md` — Nostr event kind allocation (20000-20005) and tag
  schema for cross-network packets.
- `docs/ARCHITECTURE.md` — core-vs-optional layering: Nostr identity,
  Reticulum bridge, Meshtastic wrapping, Arch OS, OSOVM tie-in.
- `docs/MESHTASTIC_ANDROID_PATTERNS.md` — patterns extracted from
  Meshtastic-Android for a future companion app (the `RadioTransport`/KMP
  shape); the full repo was cloned, checked, and removed — not kept as a
  submodule. The firmware side is a real fork, not a submodule — see
  `cryptonomicsed-byte/omokoda-mesh-firmware` and `docs/ARCHITECTURE.md` §3.

## Build

```
pio run -e generic_esp32_sx1262
```

## Related

- `omokoda-mesh-os` — Arch Linux foundation OS for dual-radio gateway
  bridge nodes (Raspberry Pi / Linux) that run the settlement/bridge
  side of this protocol.
