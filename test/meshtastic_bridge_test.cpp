// Host test for lib/meshtastic_bridge. No Arduino, no PlatformIO -- plain
// C++17, so the wire logic can be exercised without hardware.
//
//   g++ -std=c++17 -Iinclude -Wall -Wextra
//       lib/packet_wrapper/envelope.cpp
//       lib/meshtastic_bridge/meshtastic_bridge.cpp
//       test/meshtastic_bridge_test.cpp -o /tmp/t && /tmp/t

#include "meshtastic/meshtastic_bridge.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace omokoda;

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char* what) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", what);
    }
}

// A hand-built encoder, so the decoder is exercised against bytes assembled
// to the spec rather than against its own output.
void putVarint(std::vector<uint8_t>& out, uint64_t v) {
    while (true) {
        uint8_t b = static_cast<uint8_t>(v & 0x7F);
        v >>= 7;
        out.push_back(v ? static_cast<uint8_t>(b | 0x80) : b);
        if (!v) return;
    }
}

void key(std::vector<uint8_t>& out, uint32_t field, uint8_t wire) {
    putVarint(out, (static_cast<uint64_t>(field) << 3) | wire);
}

void fixed32(std::vector<uint8_t>& out, uint32_t field, uint32_t value) {
    key(out, field, 5);
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
}

void uintField(std::vector<uint8_t>& out, uint32_t field, uint64_t value) {
    key(out, field, 0);
    putVarint(out, value);
}

void bytesField(std::vector<uint8_t>& out, uint32_t field, const std::string& value) {
    key(out, field, 2);
    putVarint(out, value.size());
    out.insert(out.end(), value.begin(), value.end());
}

std::vector<uint8_t> buildPacket(uint32_t packetId = 0x1A2B3C4D,
                                 uint32_t fromNode = 0xDEADBEEF, uint8_t hopStart = 5,
                                 uint8_t hopLimit = 3, const std::string& payload = "hello mesh",
                                 bool encrypted = false, uint32_t extraField = 0) {
    std::vector<uint8_t> out;
    fixed32(out, kMeshtasticFieldFrom, fromNode);
    fixed32(out, kMeshtasticFieldTo, 0xFFFFFFFFu);
    fixed32(out, kMeshtasticFieldId, packetId);
    fixed32(out, kMeshtasticFieldRxTime, 1756000000u);
    if (hopLimit) uintField(out, kMeshtasticFieldHopLimit, hopLimit);
    if (hopStart) uintField(out, kMeshtasticFieldHopStart, hopStart);

    if (encrypted) {
        bytesField(out, kMeshtasticFieldEncrypted, std::string(8, '\0'));
    } else {
        std::vector<uint8_t> data;
        uintField(data, kMeshtasticDataPortnum, 1);
        bytesField(data, kMeshtasticDataPayload, payload);
        key(out, kMeshtasticFieldDecoded, 2);
        putVarint(out, data.size());
        out.insert(out.end(), data.begin(), data.end());
    }
    if (extraField) uintField(out, extraField, 7);
    return out;
}

void decoding() {
    std::printf("decoding\n");
    MeshtasticFrame frame;
    const auto raw = buildPacket();
    check(meshtasticDecode(raw.data(), raw.size(), frame), "a well-formed frame decodes");
    check(frame.packetId == 0x1A2B3C4D, "the packet id is read");
    check(frame.fromNode == 0xDEADBEEF, "the sending node is read");
    check(std::string(frame.payload.begin(), frame.payload.end()) == "hello mesh",
          "the payload is read");
    check(frame.portnum == 1, "the port number is read");
    check(!frame.encrypted, "a decoded frame is not marked encrypted");
}

void unknownFields() {
    std::printf("unknown fields\n");
    // A newer Meshtastic firmware adding a field must not break this.
    MeshtasticFrame frame;
    const auto raw = buildPacket(0x1A2B3C4D, 0xDEADBEEF, 5, 3, "hi", false, 99);
    check(meshtasticDecode(raw.data(), raw.size(), frame), "an unknown field is skipped");
    check(frame.packetId == 0x1A2B3C4D, "and the known fields still decode");
}

void encryptedFrames() {
    std::printf("encrypted frames\n");
    // The other arm of the payload_variant oneof: recorded, not opened.
    MeshtasticFrame frame;
    const auto raw = buildPacket(0x11112222, 0x33334444, 3, 3, "", true);
    check(meshtasticDecode(raw.data(), raw.size(), frame), "an encrypted frame decodes its header");
    check(frame.encrypted, "and is marked encrypted");
    check(frame.payload.empty(), "with no plaintext payload");
}

void truncation() {
    std::printf("truncation\n");
    const auto raw = buildPacket();
    MeshtasticFrame frame;
    check(!meshtasticDecode(raw.data(), 1, frame), "a frame cut mid-field is refused");
    check(!meshtasticDecode(raw.data(), 3, frame), "and again at another mid-field cut");
    check(!meshtasticDecode(raw.data(), raw.size() - 1, frame), "and one byte short at the end");

    // Protobuf has no terminator, so a cut exactly between fields decodes
    // cleanly as a shorter message. Only the required-field check catches it.
    check(!meshtasticDecode(raw.data(), 5, frame),
          "a frame cut on a field boundary is refused for missing an id");
    check(!meshtasticDecode(nullptr, 0, frame), "an empty frame is refused");
}

void malformed() {
    std::printf("malformed frames\n");
    MeshtasticFrame frame;

    auto tooManyHops = buildPacket(1, 2, 9, 9);
    check(!meshtasticDecode(tooManyHops.data(), tooManyHops.size(), frame),
          "a hop limit above the protocol maximum is refused");

    auto inverted = buildPacket(1, 2, 2, 4);
    check(!meshtasticDecode(inverted.data(), inverted.size(), frame),
          "a hop limit above hop start is refused");

    auto noSender = buildPacket(0x1A2B3C4D, 0);
    check(!meshtasticDecode(noSender.data(), noSender.size(), frame),
          "a frame naming no sending node is refused");

    std::vector<uint8_t> zeroField;
    key(zeroField, 0, 0);
    putVarint(zeroField, 1);
    check(!meshtasticDecode(zeroField.data(), zeroField.size(), frame),
          "field number zero is refused");

    std::vector<uint8_t> longVarint;
    key(longVarint, 9, 0);
    for (int i = 0; i < 12; ++i) longVarint.push_back(0xFF);
    longVarint.push_back(0x01);
    check(!meshtasticDecode(longVarint.data(), longVarint.size(), frame),
          "an overlong varint is refused");
}

void hopCounting() {
    std::printf("hop counting\n");
    MeshtasticFrame frame;
    const auto raw = buildPacket(1, 2, 5, 3);
    check(meshtasticDecode(raw.data(), raw.size(), frame), "the frame decodes");
    check(meshtasticHopsTaken(frame) == 2, "hops taken is hop_start minus hop_limit");

    MeshtasticFrame old;
    const auto oldRaw = buildPacket(1, 2, 0, 3);
    check(meshtasticDecode(oldRaw.data(), oldRaw.size(), old), "an older frame decodes");
    check(meshtasticHopsTaken(old) == -1,
          "a frame without hop_start reports unknown, not zero");
}

void roundTrip() {
    std::printf("round trip\n");
    MeshtasticFrame original;
    const auto raw = buildPacket(0x0A0B0C0D, 0x01020304, 4, 1, "round trip");
    check(meshtasticDecode(raw.data(), raw.size(), original), "the original decodes");

    const auto reencoded = meshtasticEncode(original);
    MeshtasticFrame again;
    check(meshtasticDecode(reencoded.data(), reencoded.size(), again), "the re-encoding decodes");
    check(again.packetId == original.packetId, "the packet id survives");
    check(again.fromNode == original.fromNode, "the sending node survives");
    check(again.hopLimit == original.hopLimit, "the hop limit survives");
    check(again.hopStart == original.hopStart, "hop start survives");
    check(again.payload == original.payload, "the payload survives");
    check(again.portnum == original.portnum, "the port number survives");
}

void envelopes() {
    std::printf("envelopes\n");
    MeshtasticFrame frame;
    const auto raw = buildPacket(0x0A0B0C0D, 0x01020304, 5, 3, "into an envelope");
    check(meshtasticDecode(raw.data(), raw.size(), frame), "the frame decodes");

    const Envelope env = meshtasticToEnvelope(frame);
    check(env.originNetwork == SourceNetwork::Meshtastic, "the origin network is set");
    check(env.originId == "!0a0b0c0d", "origin id is the packet id in the form users see");
    check(env.originPubkeyHex.empty(),
          "the node number is NOT passed off as a public key");
    check(env.hopTrail.empty(), "no hop trail: Meshtastic relays do not sign");
    check(env.hopCount == 2, "the hop count comes from the frame header");
    check(env.nostrKind == 20000, "it bridges as a mesh packet route");
    check(std::string(env.payload.begin(), env.payload.end()) == "into an envelope",
          "the payload is carried");

    MeshtasticFrame back;
    check(envelopeToMeshtastic(env, back), "the envelope converts back");
    check(back.packetId == frame.packetId, "the packet id is recovered");
    check(back.fromNode == 0,
          "fromNode is left for the caller: an envelope carries no node number");
    check(back.toNode == 0xFFFFFFFFu, "and the destination defaults to broadcast");

    // A frame the caller forgot to address must not go on the air.
    const auto unaddressed = meshtasticEncode(back);
    MeshtasticFrame rejected;
    check(!meshtasticDecode(unaddressed.data(), unaddressed.size(), rejected),
          "an unaddressed frame is refused rather than transmitted");

    Envelope foreign;
    foreign.originNetwork = SourceNetwork::Reticulum;
    foreign.originId = "!0a0b0c0d";
    check(!envelopeToMeshtastic(foreign, back),
          "an envelope from another network does not convert");

    Envelope malformedId;
    malformedId.originNetwork = SourceNetwork::Meshtastic;
    malformedId.originId = "not-an-id";
    check(!envelopeToMeshtastic(malformedId, back), "a malformed origin id does not convert");
}

void nodeIds() {
    std::printf("node ids\n");
    check(meshtasticNodeId(0x0A0B0C0D) == "!0a0b0c0d", "small numbers are zero padded");
    check(meshtasticNodeId(0xFFFFFFFFu) == "!ffffffff", "the broadcast address formats");
    check(meshtasticNodeId(0) == "!00000000", "zero formats without truncating");
}

}  // namespace

int main() {
    decoding();
    unknownFields();
    encryptedFrames();
    truncation();
    malformed();
    hopCounting();
    roundTrip();
    envelopes();
    nodeIds();

    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
