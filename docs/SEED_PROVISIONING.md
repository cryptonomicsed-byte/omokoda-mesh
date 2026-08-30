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

## Where the seed goes once provisioned (wired 2026-08-29)

`src/main.cpp`'s `setup()` now runs the real boot path: check
`NvsMasterSeedSource::getSeed()`; if absent, enter a bounded provisioning
window (`SerialSeedProvisioner::provisionFromStream()`,
`OMOKODA_SEED_PROVISION_TIMEOUT_MS`, default 5s); if a seed is available
either way, construct `DerivedSecp256k1KeySource` (Nostr identity) and call
`deriveRnsIdentity()` (RNS transport identity) from it, at the node index
described below. If no seed is available after the provisioning window,
the node falls back to `NvsSecp256k1KeySource` (random key) so it still
boots usable for testing, with a loud log line making clear this isn't a
real derived identity.

### Per-node index (implemented 2026-08-29)

Chose MAC-derived over the other two candidates (sequential flashing-time
counter, operator-assigned) because both of those need an external
registry mapping chips to indices — exactly the kind of coordination
dependency a field-deployed mesh node shouldn't require just to boot.

`deriveNodeIndexFromHardwareId()` (`include/nostr/node_index.h`,
`lib/crypto_secp256k1/node_index.cpp`) derives the index from the ESP32's
factory-burned eFuse MAC (`ESP.getEfuseMac()`) — globally unique per chip
(Espressif-guaranteed), permanent, requires zero external bookkeeping. The
raw MAC is hashed (SHA-256, low 31 bits kept) rather than used directly,
since a raw MAC's low bits cluster by manufacturer OUI prefix and would
bias the derived index distribution across a fleet.

`-D OMOKODA_NODE_INDEX_OVERRIDE=<n>` remains an explicit escape hatch
(deterministic testing, or a chip needing a pinned index for some other
reason) — normal field builds should not set this.

**Trade-off, stated plainly**: this makes node index a function of which
physical chip is running, not an operator-chosen number — a node's
identity is tied to its hardware in a way a sequential counter wouldn't
be. If a board is ever replaced, the replacement gets a *different* node
index (different MAC) and therefore a different derived key — re-flashing
the same seed onto new hardware does not preserve node identity. This
wasn't treated as a blocker (matches how BIP-32 wallets generally treat
"new device = new derivation context" as expected, not exceptional) but
is worth knowing before relying on it.

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
