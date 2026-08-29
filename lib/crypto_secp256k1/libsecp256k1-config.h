#ifndef OMOKODA_SECP256K1_CONFIG_H
#define OMOKODA_SECP256K1_CONFIG_H

// Our own libsecp256k1-config.h, deliberately separate from the one
// bundled in vendor/secp256k1-embedded/src/ (which only enables
// ECDH+RECOVERY). We need EXTRAKEYS + SCHNORRSIG for NIP-01/BIP-340
// (x-only pubkeys, Schnorr signing over the 32-byte event id) — see
// lib/nostr_bridge for where the signature actually gets consumed.
//
// This file must be the ONE libsecp256k1-config.h visible on the build's
// include path. Do NOT add vendor/secp256k1-embedded/src/ (the wrapper's
// own src dir) to any lib_extra_dirs/include path — that directory holds
// a competing copy of this filename and the two must never both be
// visible at once. We only reference the raw upstream submodule at
// vendor/secp256k1-embedded/secp256k1/ directly (see
// secp256k1_bundle.c), never the embedded-wrapper's own bundle.

#define ENABLE_MODULE_EXTRAKEYS 1
#define ENABLE_MODULE_SCHNORRSIG 1

// Field/scalar backend + build settings, same choices the vendored
// wrapper uses (portable, no external asm/GMP — required for ESP32/Xtensa
// and RISC-V targets alike).
#define USE_NUM_NONE 1
#define USE_FIELD_INV_BUILTIN 1
#define USE_SCALAR_INV_BUILTIN 1
#define USE_FIELD_10X26 1
#define USE_SCALAR_8X32 1

#define USE_ECMULT_STATIC_PRECOMPUTATION 1
#define ECMULT_GEN_PREC_BITS 4
#define ECMULT_WINDOW_SIZE 4

#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1

#endif // OMOKODA_SECP256K1_CONFIG_H
