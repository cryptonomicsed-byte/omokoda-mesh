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

## 3. Meshtastic core is wrapped, not reimplemented

`meshtastic/firmware` and `meshtastic/Meshtastic-Android` are vendored as
git submodules under `vendor/` for protocol reference and passive-sniffing
compatibility — **not forked, not modified, not the basis for our own
firmware.** Our firmware wraps/observes Meshtastic traffic (promiscuous
frame ingestion, see `RadioHAL::supportsPromiscuous()`) and translates it
through `lib/meshtastic_bridge` (still a stub) into an `Envelope`, the same
way Reticulum and LoRaWAN frames get translated. Meshtastic nodes on the
existing public mesh are never required to run our firmware or know we
exist — we're compatible with them, we don't require them to change.

Update submodules deliberately, not automatically:
```
git submodule update --remote vendor/meshtastic-firmware
git submodule update --remote vendor/meshtastic-android
```

## 4. Arch Linux is the foundation OS

Unchanged from `omokoda-mesh-os` — gateways boot Arch (archiso-based), not
a Meshtastic-vendor OS or a general Linux distro. This is where the
Meshtastic protobuf decoding, Reticulum bridge, and Nostr relay-forwarding
actually run as services, using the vendored Meshtastic protobuf
definitions from `vendor/meshtastic-firmware/protobufs/` as the wire
schema for that one bridge module — nothing else in the OS depends on
Meshtastic code.

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

## Summary table

| Layer | Role | Status |
|---|---|---|
| Nostr | Core identity, root of trust | Core |
| Reticulum | Optional offline/mesh transport | Optional bridge (stub) |
| Meshtastic | Wrapped for compatibility, vendored reference only | Wrapped, not core (stub bridge) |
| Arch Linux | Foundation OS for gateways | Core (`omokoda-mesh-os`) |
| OSOVM (Julia sim + Gaussian splatting) | Downstream consumer of kind-20001 telemetry | External, out of scope here |
