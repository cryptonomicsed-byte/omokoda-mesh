# omokoda-mesh Nostr event kinds

Every packet this mesh handles — regardless of which physical network it
entered on — is canonicalized into an `Envelope` (see
`include/packet/envelope.h`) and, when bridged onto Nostr, serialized as one
of the kinds below. Foreign networks preserve their own native framing when
bridged the other direction (Reticulum destination hashes, Meshtastic
protobufs, etc.) — the Nostr kind is the canonical form when the event
crosses onto a Nostr relay.

This scheme intentionally reuses the identity and signing model already
locked for the OSOVM witness-attestation architecture (Reticulum for
identity/signing, Nostr/Bondhive pattern for economics/reputation). It does
not introduce a second keypair scheme — node identity keys are the same
class of secp256k1/Ed25519 keys used elsewhere in the ecosystem
(BIPON39/NIP-06 derivation), so a gateway node's relay key can be the same
key material an Omo-Koda2 agent already holds.

| Kind  | Name                     | Purpose                                                                 |
|-------|--------------------------|--------------------------------------------------------------------------|
| 20000 | Mesh Packet Route        | A relayed data packet, with `hopTrail` signatures appended per hop.      |
| 20001 | GNSS / Telemetry         | Location or sensor telemetry from a node or bridged DePIN source.        |
| 20002 | Bandwidth / Proof-of-Relay | Batched hop-signature chain submitted by a gateway for settlement.    |
| 20003 | Node Presence / Announce | Capability + radio + battery announcement (discovery).                   |
| 20004 | Cross-Network Bridge Tag | Marks an event's true origin network + native ID (see tags below).       |
| 20005 | Reserved                 | Unallocated — reserve before use, document here first.                   |

## Required tags

- `["network", "<nostr|reticulum|meshtastic|lorawan|depin|local>"]` — origin network, always present.
- `["hop", "<n>"]` — current hop count.
- `["reticulum_dest", "<16byte_hash_hex>"]` — present only when `network == reticulum`.
- `["origin_id", "<native id/hash>"]` — the frame's ID in its native network, opaque outside that network.

## Hop signature format

Each relaying node appends one `HopSignature` (see `envelope.h`) containing
its pubkey, a signature over `(envelope minus this hop, timestamp)`, and a
timestamp. Gateways batch these into Merkle/ZK proofs for on-chain
settlement — that batching format is out of scope for the firmware repo and
lives in the (not-yet-created) settlement/bridge service.

## Open items before Phase 3 (ZK settlement)

- Reconcile hop-signature key material with Omo-Koda2's existing wallet
  tooling instead of minting a parallel keystore.
- Decide whether kind 20002 batches settle through the existing Sui-stack
  path already used elsewhere in the ecosystem, or a separate contract.
