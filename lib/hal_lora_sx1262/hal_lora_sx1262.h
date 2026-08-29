#pragma once
#include "hal/radio_hal.h"
#include <RadioLib.h>

// SX1262 RadioHAL implementation, built on RadioLib. Pin assignments come
// from include/hal/board_pins.h — this file never hardcodes pin numbers.

namespace omokoda {

class Sx1262RadioHAL : public RadioHAL {
public:
    Sx1262RadioHAL();

    bool begin(const RadioConfig& cfg) override;
    bool send(const uint8_t* data, size_t len) override;
    size_t receive(uint8_t* buf, size_t maxLen) override;

    bool supportsPromiscuous() const override { return false; }

    RadioKind kind() const override { return RadioKind::LoRa_SX1262; }
    int16_t lastRssi() const override { return _lastRssi; }

private:
    Module _module;
    SX1262 _radio;
    int16_t _lastRssi = 0;
    bool _receiving = false;

    bool startReceive();
};

} // namespace omokoda
