# Reticulum bridge

Implements the "Reticulum is an optional bridge, not core" decision
(`docs/ARCHITECTURE.md` §2): a Reticulum destination hash is envelope
metadata, never an identity. Nostr remains the sole root of trust — see
`docs/EVENT_KINDS.md`.

## What's implemented

- `include/reticulum/rns_packet.h` + `lib/reticulum_bridge/rns_packet.cpp`
  — `RnsPacket` struct modeling the RNS wire packet (header flags byte,
  hop count, 16-byte destination hash, optional 16-byte transport id,
  context byte, payload) with `serialize()`/`deserialize()`.
- `include/reticulum/reticulum_bridge.h` +
  `lib/reticulum_bridge/reticulum_bridge.cpp` — `rnsPacketToEnvelope()` /
  `envelopeToRnsPacket()` conversion functions, following the same shape as
  the Nostr bridge (`lib/nostr_bridge`).
- `rnsNameHash()` — truncated-SHA256 destination hash for named
  destinations (`app_name.aspect1.aspect2` → 16-byte hash), used when
  bridging a non-Reticulum envelope onto a Reticulum interface.

## Known gaps — read before treating this as interop-ready

This was built from the public RNS packet spec description, not validated
against a real `rnsd` or RNode session yet. Specifically:

1. **Header byte 0 bit packing is simplified.** Real RNS packs IFAC flag,
   header type, a context flag, propagation type, destination type, and
   packet type into one byte with specific bit positions. This
   implementation only models the fields the bridge actually branches on
   (IFAC, header type, destination type) and folds packet-type
   disambiguation into the context byte instead of the header byte. If
   real RNS nodes reject or misinterpret these packets, this bit layout is
   the first thing to check against the actual RNS source/spec.
2. **No encryption/signing.** Real Reticulum destinations of type
   Single/Link are encrypted end-to-end (X25519 + AES, or Ed25519-signed
   announces) using an RNS Identity keypair. This bridge does not
   implement RNS Identity at all — it only handles the packet *shape*, not
   the cryptographic envelope real single/link destinations require. Until
   that's added, this bridge can meaningfully interoperate with **Plain**
   destinations (unencrypted, e.g. broadcast/announce-style traffic) but
   not encrypted Single/Link traffic.
3. **No transport-layer routing/path discovery.** Real RNS transport nodes
   maintain routing tables and path announces; this bridge is packet-shape
   translation only, one hop at a time — it doesn't implement RNS's
   transport/path-finding logic.
4. **`createdAt` has no source.** RNS packets carry no timestamp field at
   this layer, so `rnsPacketToEnvelope()` currently zeros it. If ordering
   matters downstream, the envelope's `createdAt` needs to come from
   wherever the packet was received (radio RX time), not the packet
   itself.

## What "done" looks like

Before calling this interop-ready: test against a real `rnsd` instance
(or the reference Python RNS stack) with actual packet captures, fix
whatever bit-layout mismatches turn up, and decide whether RNS Identity
support (item 2 above) is in scope or whether this bridge stays
Plain-destination-only by design.
