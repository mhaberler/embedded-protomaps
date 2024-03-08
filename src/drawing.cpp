#include "FS.h"
#include <SdFat.h>
#include <M5Unified.h>
#include <M5GFX.h>

#include "logging.hpp"
#include "protomap.hpp"

#define display M5.Display

void draw_tile(tile_t *tile) {
    display.fillScreen(TFT_BLACK);
    display.setAddrWindow(0, 0, display.width(), display.height());
   
    display.startWrite(); // Occupy BUS

    // FIXME look into using pushImage
    // //M5_LOGI("oy:%d y:%d h:%d yy:%d", oy, y, h, yy);
    // me->_lcd->pushImageDMA(me->_jpg_x,  me->_jpg_y + yy,
    //                        me->_out_width, h,
    //                        reinterpret_cast<::lgfx::swap565_t*>(me->_dmabuf));

    for (int row = 0; row < tile->width; row++) {
        for (int col = 0; col < tile->height; col++) {
            size_t i = col + tile->width * row;
            uint16_t color = display.color565(tile->buffer[i], tile->buffer[i+1], tile->buffer[i+2]);
            display.pushBlock(color, 1);	// PDQ_GFX has count
        }
    }
    display.endWrite(); // Release BUS
}


void setup_draw(void) {
    display.begin();

    display.setEpdMode(epd_mode_t::epd_fastest);
    if (display.width() > display.height()) {
        display.setRotation(display.getRotation() ^ 1);
    }
    display.setBrightness(255);
    display.setFont(&Font2);
    display.setTextDatum(textdatum_t::top_center);
    display.clear(0);

    display.fillScreen(TFT_GREEN);
}

void loop_draw(void) {

}
