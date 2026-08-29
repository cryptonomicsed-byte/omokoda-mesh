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
  JSON): **implemented** (`lib/nostr_bridge`), except actual schnorr
  signing — `NostrSigner` is an interface only; the concrete implementation
  should wrap whatever BIPON39/NIP-06-derived key material the node already
  holds rather than mint a new keystore (see `docs/EVENT_KINDS.md`).
- SX1276, CC1101, BLE, WiFi HALs and the Reticulum/Meshtastic bridges:
  **stubs, not yet implemented**.
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
  id/canonical serialization, implemented (signing interface only).
- `lib/reticulum_bridge`, `lib/meshtastic_bridge` — stubs, not yet
  implemented.
- `docs/EVENT_KINDS.md` — Nostr event kind allocation (20000-20005) and tag
  schema for cross-network packets.
- `docs/ARCHITECTURE.md` — core-vs-optional layering: Nostr identity,
  Reticulum bridge, Meshtastic wrapping, Arch OS, OSOVM tie-in.
- `vendor/meshtastic-android` — git submodule, upstream reference only
  (not forked). The firmware side is now a real fork instead of a
  submodule — see `cryptonomicsed-byte/omokoda-mesh-firmware` and
  `docs/ARCHITECTURE.md` §3.

## Build

```
pio run -e generic_esp32_sx1262
```

## Related

- `omokoda-mesh-os` — Arch Linux foundation OS for dual-radio gateway
  bridge nodes (Raspberry Pi / Linux) that run the settlement/bridge
  side of this protocol.
