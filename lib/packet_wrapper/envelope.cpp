#include "packet/envelope.h"
#include <cstring>

// On-air binary form of Envelope — compact, fixed-order fields, used for
// the actual LoRa/BLE/serial transmission. This is NOT the Nostr JSON
// event form; lib/nostr_bridge converts between this and a signed
// kind-2000x Nostr event (see docs/EVENT_KINDS.md) only at network
// boundaries where a Nostr relay is actually involved.
//
// Wire layout (little-endian):
//   [1B originNetwork][1B originId len][originId bytes]
//   [1B originPubkey len][originPubkey bytes]
//   [4B createdAt][1B hopCount]
//   [1B hopTrail count] { [1B pubkey len][pubkey][1B sig len][sig][4B ts] } *count
//   [2B nostrKind]
//   [2B payload len][payload bytes]

namespace omokoda {

namespace {

void putU8(std::vector<uint8_t>& out, uint8_t v) { out.push_back(v); }

void putU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void putU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

void putStr8(std::vector<uint8_t>& out, const std::string& s) {
    uint8_t len = static_cast<uint8_t>(s.size() > 255 ? 255 : s.size());
    putU8(out, len);
    out.insert(out.end(), s.begin(), s.begin() + len);
}

bool getU8(const uint8_t* d, size_t len, size_t& pos, uint8_t& out) {
    if (pos + 1 > len) return false;
    out = d[pos++];
    return true;
}

bool getU16(const uint8_t* d, size_t len, size_t& pos, uint16_t& out) {
    if (pos + 2 > len) return false;
    out = static_cast<uint16_t>(d[pos]) | (static_cast<uint16_t>(d[pos + 1]) << 8);
    pos += 2;
    return true;
}

bool getU32(const uint8_t* d, size_t len, size_t& pos, uint32_t& out) {
    if (pos + 4 > len) return false;
    out = 0;
    for (int i = 0; i < 4; ++i) out |= static_cast<uint32_t>(d[pos + i]) << (8 * i);
    pos += 4;
    return true;
}

bool getStr8(const uint8_t* d, size_t len, size_t& pos, std::string& out) {
    uint8_t slen;
    if (!getU8(d, len, pos, slen)) return false;
    if (pos + slen > len) return false;
    out.assign(reinterpret_cast<const char*>(d + pos), slen);
    pos += slen;
    return true;
}

} // namespace

std::vector<uint8_t> Envelope::serialize() const {
    std::vector<uint8_t> out;
    putU8(out, static_cast<uint8_t>(originNetwork));
    putStr8(out, originId);
    putStr8(out, originPubkeyHex);
    putU32(out, createdAt);
    putU8(out, hopCount);

    uint8_t hopN = static_cast<uint8_t>(hopTrail.size() > 255 ? 255 : hopTrail.size());
    putU8(out, hopN);
    for (uint8_t i = 0; i < hopN; ++i) {
        const auto& h = hopTrail[i];
        putStr8(out, h.relayPubkeyHex);
        putStr8(out, h.signatureHex);
        putU32(out, h.timestamp);
    }

    putU16(out, nostrKind);

    uint16_t plen = static_cast<uint16_t>(payload.size() > 65535 ? 65535 : payload.size());
    putU16(out, plen);
    out.insert(out.end(), payload.begin(), payload.begin() + plen);

    return out;
}

bool Envelope::deserialize(const uint8_t* data, size_t len, Envelope& out) {
    size_t pos = 0;
    uint8_t netByte;
    if (!getU8(data, len, pos, netByte)) return false;
    out.originNetwork = static_cast<SourceNetwork>(netByte);

    if (!getStr8(data, len, pos, out.originId)) return false;
    if (!getStr8(data, len, pos, out.originPubkeyHex)) return false;
    if (!getU32(data, len, pos, out.createdAt)) return false;
    if (!getU8(data, len, pos, out.hopCount)) return false;

    uint8_t hopN;
    if (!getU8(data, len, pos, hopN)) return false;
    out.hopTrail.clear();
    out.hopTrail.reserve(hopN);
    for (uint8_t i = 0; i < hopN; ++i) {
        HopSignature h{};
        if (!getStr8(data, len, pos, h.relayPubkeyHex)) return false;
        if (!getStr8(data, len, pos, h.signatureHex)) return false;
        if (!getU32(data, len, pos, h.timestamp)) return false;
        out.hopTrail.push_back(std::move(h));
    }

    if (!getU16(data, len, pos, out.nostrKind)) return false;

    uint16_t plen;
    if (!getU16(data, len, pos, plen)) return false;
    if (pos + plen > len) return false;
    out.payload.assign(data + pos, data + pos + plen);

    return true;
}

} // namespace omokoda
