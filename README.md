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

Doc + scaffold stage. Nothing here talks to real radio hardware yet.

## Design principles

1. **One firmware core, many boards.** Board/radio differences live only in
   the HAL layer (`include/hal/radio_hal.h`, `lib/hal_*`). `src/main.cpp`
   never branches on board identity.
2. **One packet shape.** Every frame — regardless of origin network — is
   canonicalized into `omokoda::Envelope` (`include/packet/envelope.h`)
   before routing, signing, or hop-chain logic touches it. Bridges convert
   in/out of this shape; nothing else needs to know the origin network.
3. **No parallel identity system.** Node signing keys reuse the same
   secp256k1/Ed25519 + BIPON39/NIP-06 derivation already used elsewhere in
   the ecosystem — see `docs/EVENT_KINDS.md`.

## Layout

- `platformio.ini` — build envs for generic ESP32/S3/C3 + known community
  boards (Heltec V3, TTGO T-Beam, Cardputer ADV). Add a board by adding an
  env, not by forking source.
- `include/hal/` — radio abstraction interface.
- `include/packet/` — cross-protocol envelope.
- `lib/hal_lora_*`, `lib/hal_ble`, `lib/hal_wifi` — per-radio HAL drivers
  (stubs, not yet implemented).
- `lib/*_bridge` — per-network bridge modules (Nostr, Reticulum,
  Meshtastic) that translate to/from `Envelope` (stubs, not yet
  implemented).
- `docs/EVENT_KINDS.md` — Nostr event kind allocation (20000-20005) and tag
  schema for cross-network packets.

## Build

```
pio run -e generic_esp32_sx1262
```

## Related

- `omokoda-mesh-os` — Arch Linux foundation OS for dual-radio gateway
  bridge nodes (Raspberry Pi / Linux) that run the settlement/bridge
  side of this protocol.
