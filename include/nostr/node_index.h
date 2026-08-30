#pragma once
#include <cstdint>

// Node-index assignment for BIP-32 derivation (m/44'/20000'/<node_index>'/...,
// see docs/EVENT_KINDS.md "Key derivation"). Answers the gap flagged when
// main.cpp's boot path was first wired: two nodes sharing a master seed
// must not derive the same key, so each needs a distinct index — but
// nothing should require an external registry mapping chips to indices,
// since that's exactly the kind of coordination dependency field-deployed
// mesh nodes shouldn't need. See docs/SEED_PROVISIONING.md "Per-node
// index" for the full reasoning.

namespace omokoda {

// Derives a node index from the ESP32's factory-burned eFuse MAC —
// globally unique per chip (Espressif-guaranteed), permanent, requires no
// external bookkeeping. Not a secret and not security-sensitive on its
// own (it's derived from a fixed hardware ID, not random) — its only job
// is keeping sibling nodes on the same master seed from colliding on the
// same BIP-32 child.
//
// The raw MAC is hashed (not used directly) to avoid the low bits of a
// MAC address clustering by manufacturer OUI prefix, which would bias the
// derived index distribution; the hash's low 31 bits are used, masked to
// stay under the 0x80000000 hardened-index boundary
// bip32DeriveHardenedChild() itself ORs in.
//
// Returns 0 on non-ESP32 builds (host-side tests, etc) — callers on real
// hardware always get a real per-chip value; this fallback exists only so
// the function is callable everywhere the rest of this module is.
uint32_t deriveNodeIndexFromHardwareId();

} // namespace omokoda
