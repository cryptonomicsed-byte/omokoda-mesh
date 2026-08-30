# omokoda-mesh Nostr event kinds

Every packet this mesh handles — regardless of which physical network it
entered on — is canonicalized into an `Envelope` (see
`include/packet/envelope.h`) and, when bridged onto Nostr, serialized as one
of the kinds below.

**Custom-fit rule (locked 2026-08-29):** this firmware is a limb of the real
Omo-Koda2/OSOVM/Vantage ecosystem, not a generic mesh protocol that happens
to share names with it. Where the ecosystem already has a real, locked event
schema for something, we reuse it — we do not invent a parallel one just
because it's convenient in firmware. Verified against
`cryptonomicsed-byte/ip-layer`, `Witness-firmware`, `minipae`, `BIPON39`
directly (not from memory) on 2026-08-29. See `docs/ARCHITECTURE.md` for the
broader layering decision this sits inside.

| Kind  | Name              | Purpose                                                                 | Status |
|-------|-------------------|--------------------------------------------------------------------------|--------|
| 20000 | Mesh Packet Route | A relayed data packet, with `hopTrail` signatures appended per hop.      | Custom — genuinely mesh-specific, no ecosystem analog exists. Provisional pending formal reservation. |
| 20001 | GNSS / Telemetry  | Location or sensor telemetry from a node or bridged DePIN source; feeds OSOVM's spatial-mining pipeline. | Custom — telemetry isn't covered by any existing ecosystem schema. Provisional. |
| **1902** | **Attestation (ip-layer canonical)** | **Proof-of-relay.** A relaying node's confirmation that it physically forwarded a given kind-20000 packet — `stance: confirm`, `e` tag pointing at the routed packet's event id. **Reused directly from `ip-layer/schemas/attestation.md`, not reinvented.** | Reused, not custom. See "Proof-of-relay via Attestation" below. |
| 20003 | Node Presence / Announce | Capability + radio + battery announcement (discovery). | Custom, provisional. |

**Removed 2026-08-29**: the earlier draft had a standalone kind 20004
"Cross-Network Bridge Tag" and a reserved-but-unused kind 20005. 20004 was
vestigial — origin-network metadata is just tags on the other kinds (see
below), it never needed its own event kind, and the shipped code never
emitted one. 20005 is dropped since there's nothing waiting to claim it;
add a new kind here with justification if one is ever actually needed,
rather than pre-reserving speculatively.

## Why 20002 (Bandwidth/Proof-of-Relay) was dropped in favor of kind 1902

The original design invented its own settlement-attestation kind. Checking
`ip-layer/schemas/attestation.md` directly found the ecosystem already has
exactly this concept, locked: kind `1902`, a regular event with
`stance: vouch|challenge|confirm`, an `e` tag pointing at the subject event,
`weight` for stake-relevant amounts, explicitly designed to be
**attestor-agnostic** (human, peer agent, or Zàngbétò all use the same
shape). A relay proof is a `confirm`-stance attestation on a kind-20000
packet event — there's no reason for mesh firmware to mint a competing
kind for the same concept. See `lib/nostr_bridge/nostr_event.h`'s
`nostrBuildRelayAttestation()` for the implementation.

## Required tags (kind 20000/20001/20003)

- `["network", "<nostr|reticulum|meshtastic|lorawan|depin|local>"]` — origin network, always present.
- `["hop", "<n>"]` — current hop count.
- `["reticulum_dest", "<16byte_hash_hex>"]` — present only when `network == reticulum`.
- `["origin_id", "<native id/hash>"]` — the frame's ID in its native network, opaque outside that network.

## Hop signature format

Each relaying node appends one `HopSignature` (see `envelope.h`) containing
its pubkey, a signature over `(envelope minus this hop, timestamp)`, and a
timestamp — this is routing-layer data carried as `hop_sig` tags directly on
the kind-20000 event, distinct from the kind-1902 settlement attestation a
gateway mints afterward. This mirrors `Witness-firmware`'s real two-layer
split (see `docs/RETICULUM_BRIDGE.md` "known gaps" #2 and
`docs/ARCHITECTURE.md`): a content-level signature (here: the hop trail
appended to the routed packet) is a different trust layer from a
transport-level identity signature (RNS Identity, not yet wired into
`reticulum_bridge`).

## Key derivation — matches the real ecosystem convention (implemented 2026-08-29)

`minipae/docs/KEY_MANAGEMENT.md` locks the actual pattern: hardened BIP-32
on secp256k1, `m/44'/<kind-number>'/<agent_index>'/<owner_index>'`, derived
from the same master BIP39 seed Omo-Koda2 already uses — minipae reuses its
own NIP-AE kind (30174) as the BIP-32 purpose segment. omokoda-mesh follows
the same convention: `m/44'/20000'/<node_index>'/0'` (own primary kind
number as the purpose segment).

**Implemented**: `include/nostr/bip32_derivation.h` +
`lib/crypto_secp256k1/bip32_derivation.cpp` reimplement BIP-32 hardened
derivation (HMAC-SHA512 master key from a `"Bitcoin seed"`-keyed HMAC,
hardened CKDpriv via `secp256k1_ec_seckey_tweak_add`) directly in C++
against the already-vendored secp256k1 — matching `BIPON39`'s
`DerivationMode::Bip32` math exactly (verified against
`BIPON39/src/derivation.rs` directly, not from memory), so the same master
seed produces cross-compatible keys. This exists because `BIPON39` is a
std-only Rust crate with no `no_std`/FFI target and can't be linked into
this firmware directly. `DerivedSecp256k1KeySource`
(`include/nostr/secp256k1_signer.h`) wires this into a real
`Secp256k1KeySource` — consumes a caller-supplied 64-byte master seed
(mnemonic-to-seed conversion stays out of scope, owned by BIPON39/ecosystem
tooling) and a `node_index`, derives at the path above. Only hardened
derivation is implemented (all segments in both our path and minipae's are
hardened; non-hardened CKDpriv needs the parent's compressed public key,
not just its private key, and nothing in this ecosystem's key paths uses it
yet).

`NvsSecp256k1KeySource` (random key, no derivation) remains available as an
explicit dev/standalone fallback — real deployments should use
`DerivedSecp256k1KeySource` instead.

## IP Root tie-in

`ip-layer/schemas/ip_root.md` locks kind `31900` for IP Root. Spatial
telemetry (kind 20001) that OSOVM turns into a Creation Receipt (kind
`1901`) eventually links back to an IP Root the same way any other
creation does — omokoda-mesh doesn't mint IP Roots or Creation Receipts
itself, it only produces the raw kind-20001 telemetry OSOVM consumes.

## Future work: real schemas already locked, not yet needed by this repo

Per owner directive (2026-08-29): wire to what already exists, don't invent
ahead of need. These are confirmed real and canonical — recorded here so
whoever builds the corresponding feature doesn't reinvent it, but nothing
below is implemented in this repo yet because the feature itself isn't
built yet.

- **Kind 1903 (Twin Binding)** — `ip-layer/schemas/twin_binding.md`
  (parameterized-replaceable, like IP Root). This is the real event kind
  for the embodiment/co-pilot pairing protocol (agent ↔ drone/robot/IoT
  session negotiation) described in the portable sovereign agent device
  spec. When that pairing protocol gets built on top of this mesh, it
  emits kind 1903 events — **do not invent a parallel
  embodiment-session/offer/accept/end schema.** Re-read
  `twin_binding.md`'s exact field list before building it.
- **NIP-44 v2** — minipae's real encryption standard (HKDF conversation
  key, ChaCha20, HMAC-SHA256), already used for engram/agent-memory
  payloads. Any future encrypted mesh payload (private embodiment
  negotiation content, private engram sync over the mesh) should use real
  NIP-44 v2, not a custom scheme — even though this repo's BIP-340 Schnorr
  work is correct for signing, encryption is a different NIP and a
  different implementation.
- **Kind 30174 (NIP-AE / minipae)** — the real portable-memory wire format,
  if firmware ever needs to read/write agent memory/engrams (birth
  records, session logs) directly rather than just producing telemetry
  for OSOVM to consume.
- **NIP-42/NIP-65** — relay auth (kind 22242) and relay-list announcement,
  if/when gateway nodes need to authenticate to or announce relays
  themselves rather than just publishing.

## Open items

- Kind 20000/20001/20003 numbers are still provisional — no formal
  ecosystem-wide kind registry exists yet (checked: none of
  Witness-firmware/ip-layer/minipae/BIPON39 maintain one, they just avoid
  collisions ad hoc). Confirm no collision before treating these as locked.
- `main.cpp`'s boot path now uses `NvsMasterSeedSource` +
  `DerivedSecp256k1KeySource`/`deriveRnsIdentity()` when a seed is
  available (provisioning via `SerialSeedProvisioner` if not), falling
  back to the random `NvsSecp256k1KeySource` only if no seed shows up
  within the provisioning window — see `docs/SEED_PROVISIONING.md`.
  **Per-node index assignment is still unresolved** (`OMOKODA_NODE_INDEX`
  defaults to 0, must be manually overridden per board — no real
  assignment scheme exists yet).
- `docs/SEED_PROVISIONING.md` Tier 1 (Cardputer-class mnemonic entry) is
  deliberately deferred — needs either a firmware-embedded BIP-39
  wordlist + PBKDF2, or a `no_std`/FFI BIPON39 target, neither of which
  exist yet.
