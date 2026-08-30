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

## Real RNS::Identity, derived not random (implemented 2026-08-29)

`deriveRnsIdentity(seed, nodeIndex, out)` in `reticulum_bridge` derives a
real `RNS::Identity` from the same 64-byte master seed
`DerivedSecp256k1KeySource` uses for the content-level Nostr identity —
not a freshly-generated random one. Because RNS identities are
Ed25519 (signing) + X25519 (encryption), a different curve family from
secp256k1, this can't reuse the Nostr keypair bit-for-bit; instead it
derives two more BIP-32 hardened children from the same seed
(`m/44'/20000'/<node_index>'/1'` for the X25519 half, `.../2'` for the
Ed25519 half) and feeds the concatenated 64 bytes into
`RNS::Identity::load_private_key()`. Verified against
`vendor/microReticulum/src/Identity.cpp`'s actual `load_private_key()`
implementation directly (confirms the exact byte layout: first 32 bytes
X25519 private key, next 32 bytes Ed25519 private seed — not assumed from
the general RNS spec description) and `Type::Identity::KEYSIZE` (512
bits = 64 bytes total).

This gives every node the **two-layer signature model**
`Witness-firmware`'s `witness_rns_identity.py` establishes as the real
ecosystem pattern: `Secp256k1NostrSigner` signs content (BIP-340 Schnorr,
NIP-01-shaped, e.g. the kind-20000 packet event or the kind-1902 relay
attestation), while the derived `RNS::Identity` signs/encrypts at the
transport layer via its own real `sign()`/`validate()`/`encrypt()`/
`decrypt()` methods — same shape as `WitnessIdentity` vs.
`RNSWitnessIdentity` in `Witness-firmware`. Both keys are rooted in one
master seed even though they're cryptographically separate — see
`deriveRnsIdentity()`'s doc comment for why that's a legitimate multi-curve
KDF pattern, not a claim the curves are unified.

## What's still not done

- **Not wired into `main.cpp` yet.** Same open question as
  `DerivedSecp256k1KeySource`: where does a real node's 64-byte master
  seed come from on boot (provisioning flow, not yet designed) — see
  `docs/EVENT_KINDS.md` "Open items". `deriveRnsIdentity()` exists and is
  callable, but nothing in the firmware calls it yet.
- **`envelopeToRnsPacket()` still takes a caller-supplied
  `RNS::Destination`** rather than constructing one from the derived
  identity itself — a node wiring this up for real traffic needs to build
  its own `RNS::Destination` from `deriveRnsIdentity()`'s output.
- **No `RNS::Transport`/routing wired in.** This bridge only converts
  packet *shapes* and now owns real identity material; it still doesn't
  run microReticulum's `Transport` (path tables, multi-hop routing).
  `-DRNS_USE_FS`/`-DRNS_PERSIST_PATHS` and a `microStore::FileSystem`
  adapter are needed for that and are deliberately not enabled yet.

## What "done" looks like

A node that owns a real `RNS::Identity` (properly derived, not random),
constructs `RNS::Destination`s from it, and can send/receive actual
`RNS::Packet`s over microReticulum's `LoRaInterface` (which itself sits on
RadioLib — the same library our `hal_lora_sx1262` already uses) — at that
point this bridge is interop-tested, not just shape-compatible.
