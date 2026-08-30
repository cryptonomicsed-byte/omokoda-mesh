# omokoda-mesh

Universal, hardware-agnostic ESP32 LoRa mesh node firmware. Nostr-native
packet format, cross-network relay (Nostr / Reticulum / Meshtastic /
LoRaWAN / DePIN), proof-of-relay hop signatures.

Part of the Ọmọ Kọ́dà / OSOVM / Vantage ecosystem — a genuine limb of that
ecosystem, not a generic mesh protocol that shares its names. Built to
reuse the ecosystem's real, locked schemas wherever one exists (checked
directly against `Witness-firmware`/`ip-layer`/`minipae`/`BIPON39`, see
`docs/ARCHITECTURE.md` §6 "Custom-fit audit"), and to stay genuinely
custom only where no ecosystem schema exists yet (mesh routing/telemetry).
This supersedes the earlier framing that positioned Reticulum as a peer
identity layer — see `docs/ARCHITECTURE.md` §1-2 for the current, locked
layering.

## Status

- SX1262 radio HAL: **implemented** (`lib/hal_lora_sx1262`, via RadioLib).
- `Envelope` binary wire format: **implemented** (`lib/packet_wrapper`).
- Nostr bridge (`Envelope` <-> signed event, NIP-01 id/canonical JSON):
  **implemented** (`lib/nostr_bridge`). Proof-of-relay reuses the
  ecosystem's real `ip-layer` Attestation schema (kind 1902,
  `nostrBuildRelayAttestation()`) instead of a custom kind — see
  `docs/EVENT_KINDS.md`.
- `NostrSigner`: **implemented for real** —
  `Secp256k1NostrSigner` (`lib/crypto_secp256k1`) does actual BIP-340
  Schnorr signing via a vendored libsecp256k1 build (`vendor/secp256k1-embedded`,
  EXTRAKEYS+SCHNORRSIG modules enabled). Key material comes from a
  pluggable `Secp256k1KeySource`; `main.cpp` currently uses
  `NvsSecp256k1KeySource` (random key, dev/standalone fallback). See
  `include/nostr/secp256k1_signer.h`. `src/main.cpp` now signs and logs a
  full Nostr event end-to-end (not yet published anywhere — no relay
  transport wired in).
- **Real BIP-32 key derivation**: **implemented** —
  `include/nostr/bip32_derivation.h` + `lib/crypto_secp256k1/bip32_derivation.cpp`
  reimplement BIP-32 hardened derivation in C++ (matching `BIPON39`'s math
  exactly), and `DerivedSecp256k1KeySource` wires it into a real key
  source at `m/44'/20000'/<node_index>'/0'`, minipae's own convention.
- **Master-seed provisioning**: **implemented and wired into `main.cpp`'s
  boot path** — see `docs/SEED_PROVISIONING.md`. `NvsMasterSeedSource`
  (persisted seed) + `SerialSeedProvisioner` (one-shot 64-byte seed
  injection over serial from a trusted provisioning host) cover Tier 2
  (headless boards, most of them). `setup()` checks for a seed, tries
  serial provisioning if absent, and derives both the Nostr key and the
  RNS identity from it when available — falling back to a random dev key
  only if no seed shows up. Tier 1 (Cardputer-class mnemonic entry)
  stays deferred. **Node index** comes from
  `deriveNodeIndexFromHardwareId()` (ESP32 eFuse MAC — globally unique
  per chip, no external registry needed); `-D OMOKODA_NODE_INDEX_OVERRIDE=<n>`
  is an explicit escape hatch for testing.
- Reticulum bridge: **implemented against the real
  [microReticulum](https://github.com/Reticulum-Community/microReticulum)
  library** (`lib/reticulum_bridge`), not a hand-rolled packet format —
  converts `Envelope` <-> real `RNS::Packet`/`RNS::Destination`.
  `deriveRnsIdentity()` derives a real `RNS::Identity` (Ed25519+X25519)
  from the same master seed as the Nostr key, giving the two-layer
  signature model `Witness-firmware` establishes as the real pattern —
  see `docs/RETICULUM_BRIDGE.md`. Not yet called from `main.cpp` (same
  master-seed-provisioning gap as `DerivedSecp256k1KeySource`). No
  `RNS::Transport`/routing wired in.
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
  (BIP-340 Schnorr via vendored libsecp256k1) + NVS dev keystore + real
  BIP-32 derivation (`bip32_derivation.cpp`) + master-seed provisioning
  (`master_seed.cpp`) + node-index assignment (`node_index.cpp`,
  MAC-derived) — see `docs/SEED_PROVISIONING.md`.
- `vendor/secp256k1-embedded` — git submodule, upstream libsecp256k1
  adapted for embedded targets; our own config header
  (`lib/crypto_secp256k1/libsecp256k1-config.h`) enables the
  EXTRAKEYS+SCHNORRSIG modules the wrapper doesn't turn on by default.
- `lib/reticulum_bridge` — `Envelope` <-> real microReticulum
  `RNS::Packet`/`RNS::Destination` conversion (no RNS Identity crypto
  wired in yet — see `docs/RETICULUM_BRIDGE.md`).
- `vendor/microReticulum` — git submodule, pinned reference copy of the
  real Reticulum C++ port actually pulled into the build via
  `platformio.ini` `lib_deps`.
- `lib/meshtastic_bridge` — stub; superseded by the
  `omokoda-mesh-firmware` fork for real Meshtastic-mesh interop.
- `docs/EVENT_KINDS.md` — Nostr event kind allocation (custom: 20000/20001/
  20003; reused: `ip-layer` Attestation kind 1902 for proof-of-relay) and
  tag schema for cross-network packets.
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
