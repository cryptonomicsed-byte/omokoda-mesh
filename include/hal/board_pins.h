#pragma once
// SX1262/SX1276 SPI control-pin maps, one block per known board. Generic
// boards use a documented default wiring; override with -D flags in
// platformio.ini if your wiring differs. This is the ONLY file that should
// know board pin numbers — HAL drivers consume these macros, never hardcode
// pins themselves.

#if defined(OMOKODA_BOARD_GENERIC_ESP32)
    #define OMOKODA_LORA_NSS    5
    #define OMOKODA_LORA_DIO1   26
    #define OMOKODA_LORA_RESET  14
    #define OMOKODA_LORA_BUSY   27

#elif defined(OMOKODA_BOARD_HELTEC_V3)
    #define OMOKODA_LORA_NSS    8
    #define OMOKODA_LORA_DIO1   14
    #define OMOKODA_LORA_RESET  12
    #define OMOKODA_LORA_BUSY   13

#elif defined(OMOKODA_BOARD_TTGO_TBEAM)
    #define OMOKODA_LORA_NSS    18
    #define OMOKODA_LORA_DIO1   33
    #define OMOKODA_LORA_RESET  23
    #define OMOKODA_LORA_BUSY   32

#elif defined(OMOKODA_BOARD_CARDPUTER_ADV)
    // M5Stack Cap LoRa-1262 module pinout on the Cardputer Adv Grove port.
    #define OMOKODA_LORA_NSS    12
    #define OMOKODA_LORA_DIO1   9
    #define OMOKODA_LORA_RESET  11
    #define OMOKODA_LORA_BUSY   13

#else
    // Generic fallback for unlisted boards — override via build_flags:
    //   -D OMOKODA_LORA_NSS=<n> -D OMOKODA_LORA_DIO1=<n> ...
    #ifndef OMOKODA_LORA_NSS
        #define OMOKODA_LORA_NSS    5
    #endif
    #ifndef OMOKODA_LORA_DIO1
        #define OMOKODA_LORA_DIO1   26
    #endif
    #ifndef OMOKODA_LORA_RESET
        #define OMOKODA_LORA_RESET  14
    #endif
    #ifndef OMOKODA_LORA_BUSY
        #define OMOKODA_LORA_BUSY   27
    #endif
#endif
