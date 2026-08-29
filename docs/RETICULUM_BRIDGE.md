# Reticulum bridge

Implements the "Reticulum is an optional bridge, not core" decision
(`docs/ARCHITECTURE.md` §2): a Reticulum destination hash is envelope
metadata, never an identity. Nostr remains the sole root of trust — see
`docs/EVENT_KINDS.md`.

## Real implementation, not hand-rolled (revised 2026-08-29)

An earlier version of this bridge modeled the RNS wire packet by hand
(`RnsPacket` struct guessing at header bit-packing from the public spec
description). That guess is gone. This now bridges to
**[microReticulum](https://github.com/Reticulum-Community/microReticulum)**
(`Reticulum-Community/microReticulum`, mirrored by Reticulum's own author
as `markqvist/reticulum-cpp`) — a real, actively-maintained C++ port of the
actual RNS stack, Apache-2.0, built for ESP32/MCU targets with working LoRa
examples already in its own repo. Found by checking whether a real
ecosystem-fit alternative existed before continuing to patch the
hand-rolled version — it did.

- `vendor/microReticulum` — pinned git submodule, kept as a readable
  reference copy. The actual build pulls microReticulum via
  `platformio.ini`'s `lib_deps` (a git URL, so PlatformIO resolves its
  declared nested dependencies — ArduinoJson, MsgPack, the `attermann/Crypto`
  fork — automatically). The submodule is not what compiles; `lib_deps` is.
- `include/reticulum/reticulum_bridge.h` +
  `lib/reticulum_bridge/reticulum_bridge.cpp` — thin conversion functions
  between `omokoda::Envelope` and microReticulum's real `RNS::Packet`/
  `RNS::Destination`/`RNS::Bytes` types. No custom packet-shape code
  remains in this repo — RNS wire-format correctness is now microReticulum's
  responsibility, not ours.

## What this fixes from the old hand-rolled version

1. **Header bit-packing** — no longer our concern; microReticulum's
   `Packet`/`Type::Packet` implement the real format.
2. **Two-layer signature model** — `Witness-firmware`'s
   `witness_rns_identity.py` established that real RNS uses a
   transport-level `Identity` (Ed25519/X25519) separate from any
   content-level signature. microReticulum's `RNS::Identity` class *is*
   that real layer — this bridge doesn't need to reimplement it.
3. **Destination hashing** — `RNS::Destination::name_hash()` (real
   implementation) replaces the hand-rolled truncated-SHA256 helper.

## What's still not done

- **No encryption/signing wired in yet.** `RNS::Identity` supports real
  encrypt/sign (Curve25519 family), but this bridge doesn't construct or
  hold a live `RNS::Identity` yet — `envelopeToRnsPacket()` takes a
  caller-supplied `RNS::Destination`, it doesn't mint identity/destination
  objects itself. A node wiring this up for real traffic needs to own an
  `RNS::Identity` (loaded via `load_private_key()` once the BIP-32
  derivation task lands — see `docs/EVENT_KINDS.md` "Key derivation" —
  not a freshly-generated one) and construct `RNS::Destination`s from it.
- **No `RNS::Transport`/routing wired in.** This bridge only converts
  packet *shapes*; it doesn't run microReticulum's `Transport` (path
  tables, multi-hop routing). `-DRNS_USE_FS`/`-DRNS_PERSIST_PATHS` and a
  `microStore::FileSystem` adapter are needed for that and are
  deliberately not enabled yet — first step is validating the shape
  conversion, not standing up a full transport node.
- **The RNS Identity and the Nostr identity are cryptographically separate
  keys** (secp256k1/Schnorr vs. Ed25519/X25519) — they can never literally
  be "the same key," only both derived from the same root secret once
  BIP-32 derivation exists. Don't try to unify them into one keypair.

## What "done" looks like

A node that owns a real `RNS::Identity` (properly derived, not random),
constructs `RNS::Destination`s from it, and can send/receive actual
`RNS::Packet`s over microReticulum's `LoRaInterface` (which itself sits on
RadioLib — the same library our `hal_lora_sx1262` already uses) — at that
point this bridge is interop-tested, not just shape-compatible.
