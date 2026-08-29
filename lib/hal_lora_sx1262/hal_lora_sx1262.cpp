#include "hal_lora_sx1262.h"
#include "hal/board_pins.h"

namespace omokoda {

Sx1262RadioHAL::Sx1262RadioHAL()
    : _module(OMOKODA_LORA_NSS, OMOKODA_LORA_DIO1, OMOKODA_LORA_RESET, OMOKODA_LORA_BUSY),
      _radio(&_module) {}

bool Sx1262RadioHAL::begin(const RadioConfig& cfg) {
    float freqMhz = cfg.frequencyHz / 1000000.0f;
    float bwKhz = static_cast<float>(cfg.bandwidthKhz);

    int state = _radio.begin(freqMhz, bwKhz, cfg.spreadingFactor,
                              cfg.codingRate, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                              cfg.txPowerDbm);
    if (state != RADIOLIB_ERR_NONE) {
        return false;
    }

    // CRC on by default; foreign-network sniffing (Meshtastic etc.) needs a
    // separate promiscuous listen path that this driver doesn't attempt —
    // that belongs in lib/meshtastic_bridge once implemented.
    _radio.setCRC(true);

    return startReceive();
}

bool Sx1262RadioHAL::startReceive() {
    int state = _radio.startReceive();
    _receiving = (state == RADIOLIB_ERR_NONE);
    return _receiving;
}

bool Sx1262RadioHAL::send(const uint8_t* data, size_t len) {
    // transmit() is blocking in RadioLib; the core loop calls this rarely
    // (relay/forward path), so blocking is acceptable for now. A
    // non-blocking startTransmit()/finishTransmit() pair can replace this
    // once the core loop needs to overlap TX with other work.
    int state = _radio.transmit(const_cast<uint8_t*>(data), len);
    startReceive(); // SX126x drops out of RX after a TX; resume listening
    return state == RADIOLIB_ERR_NONE;
}

size_t Sx1262RadioHAL::receive(uint8_t* buf, size_t maxLen) {
    if (!_receiving) {
        return 0;
    }

    size_t len = _radio.getPacketLength();
    if (len == 0) {
        return 0; // nothing pending
    }
    if (len > maxLen) {
        len = maxLen; // truncate rather than overflow caller's buffer
    }

    int state = _radio.readData(buf, len);
    _lastRssi = static_cast<int16_t>(_radio.getRSSI());

    startReceive(); // re-arm for the next packet

    return (state == RADIOLIB_ERR_NONE) ? len : 0;
}

} // namespace omokoda

// Selected by the active PlatformIO environment (OMOKODA_RADIO_SX1262).
// This is the one place a radio choice becomes a concrete HAL instance —
// main.cpp never sees a radio type name.
#if defined(OMOKODA_RADIO_SX1262)
omokoda::RadioHAL& getActiveRadioHAL() {
    static omokoda::Sx1262RadioHAL instance;
    return instance;
}
#endif
