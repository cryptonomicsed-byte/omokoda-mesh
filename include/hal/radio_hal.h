#pragma once
#include <cstdint>
#include <cstddef>

// Universal radio abstraction. Every transceiver driver (SX1262, SX1276,
// CC1101, BLE, WiFi-ESPNOW) implements this interface. The core packet
// engine never talks to radio chips directly — only through RadioHAL.
// This is what makes the firmware board-agnostic: adding a new radio
// means writing one driver, not touching main.cpp or the packet engine.

namespace omokoda {

enum class RadioKind : uint8_t {
    LoRa_SX1262 = 0,
    LoRa_SX1276 = 1,
    SubGHz_CC1101 = 2,
    BLE = 3,
    WiFi = 4,
    Serial = 5,
    Cellular = 6,
};

struct RadioConfig {
    uint32_t frequencyHz;      // e.g. 915000000
    uint8_t spreadingFactor;   // LoRa SF7-SF12, ignored for non-LoRa
    uint8_t bandwidthKhz;      // e.g. 125
    uint8_t codingRate;        // 4/5..4/8
    int8_t txPowerDbm;
};

class RadioHAL {
public:
    virtual ~RadioHAL() = default;

    virtual bool begin(const RadioConfig& cfg) = 0;
    virtual bool send(const uint8_t* data, size_t len) = 0;

    // Non-blocking receive. Returns bytes read, 0 if nothing pending.
    virtual size_t receive(uint8_t* buf, size_t maxLen) = 0;

    // True if this HAL can passively sniff foreign-network frames
    // (e.g. Meshtastic broadcasts) without joining that network.
    virtual bool supportsPromiscuous() const { return false; }
    virtual void setPromiscuous(bool on) { (void)on; }

    virtual RadioKind kind() const = 0;
    virtual int16_t lastRssi() const = 0;
};

} // namespace omokoda
