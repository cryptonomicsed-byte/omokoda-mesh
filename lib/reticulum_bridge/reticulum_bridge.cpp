#include "reticulum/reticulum_bridge.h"

namespace omokoda {

bool rnsPacketToEnvelope(const RNS::Packet& pkt, Envelope& out) {
    out.originNetwork = SourceNetwork::Reticulum;
    out.originId = pkt.destination_hash().toHex();
    out.originPubkeyHex.clear(); // RNS transport identity is a separate
                                  // crypto domain from the Nostr identity
                                  // (see reticulum_bridge.h) — caller
                                  // attaches a Nostr pubkey from context
                                  // if this packet is being re-emitted as
                                  // a kind-20000 event.
    out.createdAt = 0; // RNS packets carry no timestamp at this layer;
                        // caller should stamp with local receive time.
    out.hopCount = pkt.hops();
    out.nostrKind = 20000; // Mesh Packet Route by default

    const RNS::Bytes& data = pkt.data();
    out.payload.assign(data.data(), data.data() + data.size());

    return true;
}

RNS::Packet envelopeToRnsPacket(const Envelope& env, const RNS::Destination& destination) {
    RNS::Bytes payload(env.payload.data(), env.payload.size());
    RNS::Packet pkt(destination, payload);
    pkt.hops(env.hopCount);
    return pkt;
}

RNS::Bytes reticulumNameHash(const char* appName, const char* aspects) {
    return RNS::Destination::name_hash(appName, aspects);
}

} // namespace omokoda
