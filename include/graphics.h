#pragma once

#ifdef M5UNIFIED
    #include <M5Unified.h>
    #include <M5GFX.h>
#endif

#ifdef LOVYANGFX
    #include <LovyanGFX.hpp>

    #if defined(ARDUINO_M5STACK_CORES3) || defined(ARDUINO_M5STACK_Core2)
        #define SCREEN_WIDTH 320
        #define SCREEN_HEIGHT 240
        #define LGFX_AUTODETECT
        #include <LGFX_AUTODETECT.hpp>
    #endif
    #if defined(ARDUINO_M5STACK_Paper)
        #define SCREEN_WIDTH 540
        #define SCREEN_HEIGHT 960
        #define LGFX_AUTODETECT
        #include <LGFX_AUTODETECT.hpp>
    #endif

#endif