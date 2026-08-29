#include "nostr_event.h"
#include <Crypto.h>
#include <SHA256.h>
#include <ArduinoJson.h>
#include <sstream>
#include <iomanip>

namespace omokoda {

namespace {

const char* networkName(SourceNetwork n) {
    switch (n) {
        case SourceNetwork::Nostr: return "nostr";
        case SourceNetwork::Reticulum: return "reticulum";
        case SourceNetwork::Meshtastic: return "meshtastic";
        case SourceNetwork::LoRaWAN: return "lorawan";
        case SourceNetwork::DePIN: return "depin";
        case SourceNetwork::Local: return "local";
    }
    return "local";
}

bool networkFromName(const std::string& s, SourceNetwork& out) {
    if (s == "nostr") { out = SourceNetwork::Nostr; return true; }
    if (s == "reticulum") { out = SourceNetwork::Reticulum; return true; }
    if (s == "meshtastic") { out = SourceNetwork::Meshtastic; return true; }
    if (s == "lorawan") { out = SourceNetwork::LoRaWAN; return true; }
    if (s == "depin") { out = SourceNetwork::DePIN; return true; }
    if (s == "local") { out = SourceNetwork::Local; return true; }
    return false;
}

std::string bytesToHex(const uint8_t* data, size_t len) {
    static const char* hexChars = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(hexChars[(data[i] >> 4) & 0xF]);
        out.push_back(hexChars[data[i] & 0xF]);
    }
    return out;
}

// Escapes a string per the NIP-01 canonical JSON rules used for id
// computation: only \\, \", \n, \r, \t, \b, \f, and control chars < 0x20
// need escaping; everything else (including UTF-8 multibyte) passes through
// verbatim.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string tagsToCanonical(const NostrEvent& ev) {
    std::string out = "[";
    for (size_t i = 0; i < ev.tags.size(); ++i) {
        if (i > 0) out += ",";
        out += "[\"" + jsonEscape(ev.tags[i].first) + "\"";
        for (const auto& v : ev.tags[i].second) {
            out += ",\"" + jsonEscape(v) + "\"";
        }
        out += "]";
    }
    out += "]";
    return out;
}

} // namespace

std::string nostrCanonicalSerialize(const NostrEvent& ev) {
    // [0, pubkey, created_at, kind, tags, content] — NIP-01, compact, no
    // insignificant whitespace.
    std::ostringstream oss;
    oss << "[0,\"" << jsonEscape(ev.pubkeyHex) << "\","
        << ev.createdAt << ","
        << ev.kind << ","
        << tagsToCanonical(ev) << ","
        << "\"" << jsonEscape(ev.content) << "\"]";
    return oss.str();
}

bool nostrFinalizeEvent(NostrEvent& ev, NostrSigner& signer) {
    ev.pubkeyHex = signer.pubkeyHex();

    std::string canonical = nostrCanonicalSerialize(ev);

    SHA256 sha;
    sha.update(reinterpret_cast<const uint8_t*>(canonical.data()), canonical.size());
    uint8_t digest[32];
    sha.finalize(digest, sizeof(digest));

    ev.id = bytesToHex(digest, sizeof(digest));
    ev.sigHex = signer.signId(ev.id);

    return !ev.sigHex.empty();
}

NostrEvent envelopeToNostrEvent(const Envelope& env, const std::string& pubkeyHex) {
    NostrEvent ev;
    ev.pubkeyHex = pubkeyHex;
    ev.createdAt = env.createdAt;
    ev.kind = env.nostrKind != 0 ? env.nostrKind : 20000; // default: Mesh Packet Route

    ev.tags.push_back({"network", {networkName(env.originNetwork)}});
    ev.tags.push_back({"hop", {std::to_string(env.hopCount)}});
    ev.tags.push_back({"origin_id", {env.originId}});
    if (env.originNetwork == SourceNetwork::Reticulum && !env.originId.empty()) {
        ev.tags.push_back({"reticulum_dest", {env.originId}});
    }
    for (const auto& h : env.hopTrail) {
        ev.tags.push_back({"hop_sig", {h.relayPubkeyHex, h.signatureHex, std::to_string(h.timestamp)}});
    }

    // Payload travels as base64 content via ArduinoJson-free minimal encode
    // isn't ideal; a full base64 helper belongs in a shared util once more
    // than one bridge needs it. For now content carries the origin pubkey
    // reference and payload length; the payload bytes themselves are only
    // meaningful to nodes that already share the envelope's binary form
    // (see envelope.cpp) — Nostr-side consumers needing the raw payload
    // should treat `content` as opaque and fetch full data via the
    // envelope binary form over the originating transport.
    ev.content = env.originPubkeyHex;

    return ev;
}

bool nostrEventToEnvelope(const NostrEvent& ev, Envelope& out) {
    bool haveNetwork = false, haveHop = false, haveOriginId = false;

    for (const auto& tag : ev.tags) {
        if (tag.first == "network" && !tag.second.empty()) {
            if (!networkFromName(tag.second[0], out.originNetwork)) return false;
            haveNetwork = true;
        } else if (tag.first == "hop" && !tag.second.empty()) {
            out.hopCount = static_cast<uint8_t>(std::stoi(tag.second[0]));
            haveHop = true;
        } else if (tag.first == "origin_id" && !tag.second.empty()) {
            out.originId = tag.second[0];
            haveOriginId = true;
        } else if (tag.first == "hop_sig" && tag.second.size() >= 3) {
            HopSignature h;
            h.relayPubkeyHex = tag.second[0];
            h.signatureHex = tag.second[1];
            h.timestamp = static_cast<uint32_t>(std::stoul(tag.second[2]));
            out.hopTrail.push_back(std::move(h));
        }
    }

    if (!haveNetwork || !haveHop || !haveOriginId) return false;

    out.originPubkeyHex = ev.content;
    out.createdAt = ev.createdAt;
    out.nostrKind = ev.kind;
    return true;
}

std::string nostrEventToJson(const NostrEvent& ev) {
    JsonDocument doc;
    doc["id"] = ev.id;
    doc["pubkey"] = ev.pubkeyHex;
    doc["created_at"] = ev.createdAt;
    doc["kind"] = ev.kind;
    JsonArray tags = doc["tags"].to<JsonArray>();
    for (const auto& tag : ev.tags) {
        JsonArray t = tags.add<JsonArray>();
        t.add(tag.first);
        for (const auto& v : tag.second) t.add(v);
    }
    doc["content"] = ev.content;
    doc["sig"] = ev.sigHex;

    std::string out;
    serializeJson(doc, out);
    return out;
}

} // namespace omokoda
