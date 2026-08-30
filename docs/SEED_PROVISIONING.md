# Master-seed provisioning

Answers the open question flagged since the BIP-32 derivation work landed:
where does a real node's 64-byte master seed (the root
`DerivedSecp256k1KeySource` and `deriveRnsIdentity()` both derive from)
actually come from at boot? Checked `minipae` and the portable sovereign
agent device spec first, per the owner's instruction — neither answers
this for omokoda-mesh's specific device class, so this is genuine new
design, grounded in what they *do* establish rather than invented from
scratch.

## What already exists, and why it doesn't directly answer this

- **Portable device birth sequence** (`omokoda-portable-device-full-spec`
  memory) — a full ceremony (hardware RNG → NIP-06 keypair → optional
  BIPON39 mnemonic shown once → IP Root genesis → Nostr announcement).
  Assumes a device with a screen, keyboard/voice input, and enough compute
  to run the whole agent runtime. A bare ESP32 LoRa relay node is a much
  smaller device class than this — most omokoda-mesh boards (generic
  ESP32/S3/C3, Heltec V3, TTGO T-Beam) have neither a screen nor a
  keyboard.
- **Buzz's "sealed-seed" NIP-01 identity** (`minipae/docs/RUNTIME_REGISTRY.md`)
  — a real precedent: `/api/agents/me/buzz/register` issues an identity
  server-side. Doesn't fit here either — it assumes live API connectivity
  at provisioning time, which a field LoRa node may not have (that's the
  whole point of a LoRa mesh node: it needs to work off-grid).

Neither precedent is wrong for what it was built for; neither fits a
headless, possibly-offline mesh node. This doc picks a design that reuses
their *shape* (seed-in, identity-out; provision-once, not per-boot) without
copying either's specific transport.

## Design: three tiers, matched to real hardware classes already locked

### Tier 1 — Cardputer-class nodes (has keyboard + screen)

Owner enters a BIPON39 mnemonic once at first boot, same shape as the
portable device's birth sequence, scaled down to just the seed step (no
full agent runtime birth — this is a mesh node, not an agent). **Deferred,
not implemented**: mnemonic-to-seed conversion is BIP-39 wordlist +
PBKDF2 logic, which stays owned by BIPON39/ecosystem tooling per the
already-established boundary (`docs/EVENT_KINDS.md` "Key derivation").
Implementing this tier for real means either embedding a wordlist and
PBKDF2-HMAC-SHA512 in firmware (real but nontrivial additional scope) or
BIPON39 growing a `no_std`/FFI target this firmware could call. Interface
declared (`CardputerMnemonicProvisioner`, see below) so this slots in
later without changing the seed-consumer side.

### Tier 2 — Generic/headless nodes (no keyboard, most boards): serial injection

**Implemented.** A trusted provisioning host (a laptop, or a gateway node
that already went through its own birth sequence) writes the 64-byte seed
to the node once over USB serial during physical setup — the lowest-common-
denominator transport every ESP32 dev board already supports, no camera/QR
or BLE pairing UX needed. See `SerialSeedProvisioner`
(`include/nostr/master_seed.h`, `lib/crypto_secp256k1/master_seed.cpp`):
a simple magic-header handshake, exactly 64 bytes read within a timeout,
written once to NVS (never silently overwritten, same pattern as
`NvsSecp256k1KeySource::provisionIfAbsent()`). BLE or QR-scanned transport
could carry the same 64 bytes later using the identical protocol — the
transport is swappable, the provisioning *protocol* (magic header, exactly
64 bytes, write-once) is what matters and is transport-agnostic by design.

### Tier 3 — Gateway nodes (`omokoda-mesh-os`, full Arch OS, real storage)

Out of scope for this firmware repo. A gateway has enough compute/storage
to run the real portable-device birth sequence itself (or defer to
Omo-Koda2's `/v1/birth` flow) and becomes the trusted provisioning host for
Tier-2 field nodes it pairs with — i.e. Tier 3 is the source, Tier 2 is a
consumer of Tier 3's output. Not designed further here; belongs to
`omokoda-mesh-os`/Omo-Koda2, not this repo.

## Where the seed goes once provisioned

`NvsMasterSeedSource` (implements `MasterSeedSource`) reads the persisted
seed from NVS on every subsequent boot. `DerivedSecp256k1KeySource` and
`deriveRnsIdentity()` both consume a raw `const uint8_t[64]` today (caller
supplies it) — wiring `NvsMasterSeedSource` into `main.cpp`'s boot path is
the next step once a node actually needs to run with derived (not random)
keys, and is intentionally left as the caller's choice of which
`MasterSeedSource` to use rather than hardcoded.

## Explicitly not solved here

- Mnemonic-to-seed conversion (Tier 1) — deferred, see above.
- Authenticating *who* is allowed to provision a node over serial (Tier 2
  currently trusts whoever has physical USB access during the
  provisioning window — matches "physical possession = trust" for initial
  setup, same assumption most IoT provisioning flows make, but worth
  revisiting if a node is provisioned somewhere the physical-access
  assumption doesn't hold).
- Re-provisioning / seed rotation flow (what happens if a node needs a new
  seed later) — only first-provisioning is handled.
