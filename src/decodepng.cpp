#include "logging.hpp"
#include "protomap.hpp"
#include "pngle.h"

static void pngle_init_cb(pngle_t *pngle, uint32_t w, uint32_t h) {
    size_t size =  w * h * 3;
    uint8_t *buffer = (uint8_t *)MALLOC(size);
    pngle_set_user_data(pngle, buffer);
}

static void pngle_draw_cb(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
    uint8_t *buffer  = (uint8_t *) pngle_get_user_data(pngle);
    size_t offset = (x + pngle_get_width(pngle)* y) * 3;
    buffer[offset] = rgba[0];
    buffer[offset+1] = rgba[1];
    buffer[offset+2] = rgba[2];
}

tileStatus_t decode_png(const uint8_t *blob, size_t blob_size, int& width, int& height, uint8_t **buffer) {
    pngle_t *pngle = pngle_new();
    pngle_set_init_callback(pngle, pngle_init_cb);
    pngle_set_draw_callback(pngle, pngle_draw_cb);
    int fed = pngle_feed(pngle, blob, blob_size);
    uint8_t *img  = (uint8_t *) pngle_get_user_data(pngle);
    if (fed != blob_size) {
        LOG_ERROR("decode failed: decoded %d out of %u: %s",
                  fed, blob_size, pngle_error(pngle));
        if (img != nullptr)
            FREE(img);
        pngle_destroy(pngle);
        return TS_PNG_DECODE_ERROR;
    }
    width = pngle_get_width(pngle);
    height = pngle_get_height(pngle);
    *buffer = img;
    pngle_destroy(pngle);
    return TS_VALID;
}