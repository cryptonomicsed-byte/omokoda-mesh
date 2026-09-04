# meshtastic_bridge

Converts between `omokoda::Envelope` and a Meshtastic `MeshPacket`, so a frame
this node hears on a Meshtastic channel becomes an envelope like any other and
travels the rest of the pipeline unchanged — on-air binary form, or a signed
kind-20000 Nostr event via `lib/nostr_bridge`.

## What is implemented

`meshtasticDecode` / `meshtasticEncode` handle the MeshPacket fields the
bridge maps: `from`, `to`, `channel`, `id`, `rx_time`, `hop_limit`,
`hop_start`, `via_mqtt`, `relay_node`, and either arm of the
`payload_variant` oneof (`decoded` — its `portnum` and `payload` — or
`encrypted`). Every other field is skipped by wire type, so newer Meshtastic
firmware adding fields does not break decoding.

Field numbers come from `meshtastic/protobufs` `mesh.proto`, read directly
(2026-09-04) rather than from memory, per this repo's custom-fit rule.

## Licence

`docs/MESHTASTIC_ANDROID_PATTERNS.md` records that Meshtastic-Android is
GPLv3 and that no code was copied from it. Nothing is copied here either.
Protobuf field numbers are the wire interface; the encoder and decoder are
written from scratch for the fields listed above. No Meshtastic source is
vendored, generated from, or linked, so nothing here carries a copyleft
obligation into this repository.

## What deliberately does not map

**Identity.** A Meshtastic frame is not signed, and its `from` is a 32-bit
node number the device assigns itself. `meshtasticToEnvelope` leaves
`originPubkeyHex` empty rather than putting the node number there, because a
consumer reading that field expects a key. A gateway signs the resulting
kind-20000 event with its own key; the node number is data about the frame,
not a claim about who sent it.

**Hop trail.** Meshtastic relays do not sign what they forward, so
`hopTrail` stays empty. What is carried is `hop_start - hop_limit`, exposed
as `meshtasticHopsTaken`, which returns `-1` — not `0` — when `hop_start` is
unset. Firmware older than `hop_start` does not send one, and reporting `0`
for those would claim every such packet arrived direct.

**The reverse direction is incomplete by construction.** `envelopeToMeshtastic`
leaves `fromNode` at 0 because an envelope carries no node number to restore;
the only correct value is the transmitting node's own. The caller sets it, and
`meshtasticDecode` refuses a frame that still holds 0, so forgetting fails
loudly instead of putting an unaddressed packet on the air.

## Truncation

Protobuf has no terminator and no length prefix, so a message cut exactly on a
field boundary decodes cleanly as a shorter message. Nothing structural
catches that. `meshtasticDecode` therefore requires the fields a real received
packet always carries — `id` and `from` — and refuses a frame missing either.

## Tests

`test/meshtastic_bridge_test.cpp` is a host test with no Arduino or PlatformIO
dependency:

```sh
g++ -std=c++17 -Iinclude -Wall -Wextra \
    lib/packet_wrapper/envelope.cpp \
    lib/meshtastic_bridge/meshtastic_bridge.cpp \
    test/meshtastic_bridge_test.cpp -o /tmp/meshtastic_bridge_test && /tmp/meshtastic_bridge_test
```
