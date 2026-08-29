// Single-translation-unit build of libsecp256k1, sized for embedded
// targets (no external asm/GMP, static ecmult precomputation). Compiles
// the real upstream secp256k1.c directly from the pinned submodule at
// vendor/secp256k1-embedded/secp256k1/ — we do NOT use the wrapper
// repo's own src/secp256k1_bundle.c because it only enables the
// ECDH+RECOVERY modules; NIP-01 needs EXTRAKEYS+SCHNORRSIG instead. See
// libsecp256k1-config.h in this directory for the module/backend flags.
//
// Reuses vendor/secp256k1-embedded/src/ecmult_static_context.h (the
// precomputed static-context table for ECMULT_GEN_PREC_BITS=4) via the
// build's include path (see platformio.ini) — that file is pure
// generated data, not a config header, so it has no naming collision
// with our own libsecp256k1-config.h. Build flags put this directory's
// config header first on the include path so ours wins, then fall
// through to the wrapper's src/ directory for that one data table.

#define SECP256K1_BUILD

#include "libsecp256k1-config.h"
#include "../../vendor/secp256k1-embedded/secp256k1/src/secp256k1.c"
