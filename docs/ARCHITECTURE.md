# Architecture — core vs. optional layers

This supersedes the informal "everything is equally universal" framing from
the original design doc. Four concrete decisions (2026-08-29), in order:

## 1. Nostr (NIP) is core identity — not Reticulum

Every node's identity is a Nostr keypair (NIP-06 derived, same class of key
as the rest of the ecosystem — see `docs/EVENT_KINDS.md`). Birth, presence,
hop signatures, and settlement all key off the npub. This was implicit
before; it's now explicit and load-bearing: **no other identity system is
allowed to become a second root.**

## 2. Reticulum is an optional bridge, not core

Reticulum remains a supported transport/interface (`lib/reticulum_bridge`,
still a stub) for delay-tolerant / offline mesh scenarios, but it is no
longer positioned as a peer identity layer alongside Nostr. A Reticulum
destination hash is metadata on an envelope (`reticulum_dest` tag), not an
alternate identity. If Reticulum is unavailable or unwanted on a given
deployment, the node loses nothing identity-wise — Nostr already covers it.

## 3. Meshtastic core is forked, not wrapped (revised 2026-08-29)

Wrapping (passive sniffing of an unmodified upstream binary/firmware) was
tried first and rejected: it's one-way — it can never let an omokoda node
**originate** a packet real Meshtastic nodes will route, only observe their
traffic. We now maintain a real fork:
**`cryptonomicsed-byte/omokoda-mesh-firmware`** (forked from
`meshtastic/firmware`). See that repo's `docs/OMOKODA_FORK_PLAN.md` for the
exact scope: only `src/mesh/CryptoEngine` is replaced (Nostr/NIP-06-rooted
keys instead of Meshtastic's own Curve25519 keypair generation); mesh
routing/flooding and radio HAL/board support stay upstream, unmodified, so
the fork remains a first-class participant on existing public Meshtastic
meshes.

`Meshtastic-Android` was cloned, checked for reusable patterns, and then
**removed** — not kept as a submodule. It turned out to already be
mid-transition to Kotlin Multiplatform (Android/iOS/desktop shared
business logic) with a clean `RadioTransport` interface unifying
BLE/Serial/TCP. That shape is documented in
`docs/MESHTASTIC_ANDROID_PATTERNS.md` for reuse whenever a companion app
gets built — the identity swap itself is firmware/protocol-level and
doesn't need a fork of the app.

`omokoda-mesh` (this repo, the from-scratch universal firmware) and
`omokoda-mesh-firmware` (the Meshtastic fork) are siblings, not
competitors — use this repo for nodes needing no Meshtastic-mesh interop,
use the fork for nodes that must be native Meshtastic-mesh participants.
Both root identity in Nostr.


## 4. Arch Linux is the foundation OS

Unchanged from `omokoda-mesh-os` — gateways boot Arch (archiso-based), not
a Meshtastic-vendor OS or a general Linux distro. This is where the
Reticulum bridge and Nostr relay-forwarding actually run as services. The
Meshtastic side runs as `omokoda-mesh-firmware` (the fork, §3) on the
actual ESP32 radio hardware, not as an OS-level service — the gateway OS
talks to that hardware over serial/BLE, it doesn't decode Meshtastic
protobufs itself.

## 5. OSOVM tie-in: Julia sim validation + Gaussian splatting

Spatial telemetry carried by kind-20001 events (GNSS/telemetry, see
`docs/EVENT_KINDS.md`) is the raw feed for OSOVM's spatial-mining pipeline:
- **Julia sim validation** — OSOVM's existing PoSim verification path
  consumes mesh-sourced telemetry/delta reports the same way it consumes
  any other work claim, producing proof-of-simulation receipts.
- **Gaussian splatting** — camera/sensor deltas mined by mesh-connected
  devices (see the portable sovereign agent device spec) feed the existing
  spatial-mining reconstruction pipeline; the mesh is just the transport
  that gets those deltas from an edge device to a gateway with enough
  compute to run the splatting/reconstruction step.

This repo does not implement Julia sim validation or Gaussian splatting —
those live in OSOVM. The mesh's job stops at delivering a signed kind-20001
event to a gateway; OSOVM picks it up from there. No new pipeline needed
here, just make sure the event schema is something OSOVM's existing
ingestion can consume without translation.

## 6. Custom-fit audit (2026-08-29): built for this ecosystem, not generic

Explicit owner directive: this firmware must be a genuine limb of
Omo-Koda2/OSOVM/Vantage, using their real, already-locked schemas — not a
generic mesh protocol that happens to share names with the ecosystem.
Verified directly against `Witness-firmware`, `ip-layer`, `minipae`,
`BIPON39` (cloned and read, not recalled from memory) on 2026-08-29.
Findings and what changed as a result — see `docs/EVENT_KINDS.md` for full
detail:

- **Proof-of-relay was reinvented, now fixed**: dropped the custom kind
  20002 "Bandwidth/Proof-of-Relay" in favor of `ip-layer`'s real,
  attestor-agnostic Attestation schema (kind `1902`, `stance: confirm`).
  See `lib/nostr_bridge`'s `nostrBuildRelayAttestation()`.
- **Vestigial custom kind removed**: kind 20004 "Cross-Network Bridge Tag"
  was never actually emitted by the code — origin-network metadata was
  already just tags on the other kinds. Dropped rather than kept as dead
  weight.
- **Key derivation still generic, needs fixing**: `minipae` locks a real
  ecosystem convention — hardened BIP-32 from the shared master BIP39
  seed, `m/44'/<kind-number>'/<agent_index>'/<owner_index>'`. Our
  `NvsSecp256k1KeySource` currently generates a random key instead of
  deriving one this way. `BIPON39` has the correct NIP-06 math but is a
  std-only Rust crate with no FFI/`no_std` target, so it can't be linked
  directly — the derivation needs reimplementing in C++ against our
  already-vendored secp256k1. Not yet done.
- **Reticulum bridge missing a real layer**: `Witness-firmware`'s
  `witness_rns_identity.py` uses the actual `RNS` library and establishes
  a two-layer signature model (content-level Schnorr + transport-level
  RNS Identity/Ed25519) that our `reticulum_bridge` doesn't yet have — see
  `docs/RETICULUM_BRIDGE.md` gap #2. It also names **microReticulum** as
  the real embedded/C target, worth evaluating against our hand-rolled
  `RnsPacket` before extending it further.
- **What was already right**: the Schnorr/NIP-01-shaped node identity
  approach (`Secp256k1NostrSigner`) is confirmed correct — it's the exact
  same primitive `Witness-firmware`'s `WitnessIdentity` uses independently
  for the same purpose. Genuinely mesh-specific kinds (20000 route, 20001
  telemetry, 20003 presence) have no ecosystem analog to reuse, so staying
  custom there is correct, not a shortcut — but still provisional pending
  a formal kind-number reservation (no ecosystem-wide registry exists to
  check against yet).

## Summary table

| Layer | Role | Status |
|---|---|---|
| Nostr | Core identity, root of trust | Core |
| Reticulum | Optional offline/mesh transport | Optional bridge (packet-shape real, missing RNS Identity layer — see §6) |
| Meshtastic | Real Meshtastic-mesh interop | Forked (`omokoda-mesh-firmware`), not wrapped — see §3 |
| Arch Linux | Foundation OS for gateways | Core (`omokoda-mesh-os`) |
| OSOVM (Julia sim + Gaussian splatting) | Downstream consumer of kind-20001 telemetry | External, out of scope here |
| ip-layer Attestation (kind 1902) | Proof-of-relay | Reused directly, not reinvented — see §6 |
