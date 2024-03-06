
#include "logging.hpp"
#include "protomap.hpp"

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

tileStatus_t decode_webp(const uint8_t *blob, size_t blob_size, int& width, int& height, uint8_t **buffer) {
    VP8StatusCode sc;
    WebPDecoderConfig config;
    size_t bufsize;

    WebPInitDecoderConfig(&config);
    int rc = WebPGetInfo(blob, blob_size, &width, &height);
    if (!rc) {
        LOG_ERROR("WebPGetInfo failed rc=%d",  rc);
        return TS_WEBP_DECODE_ERROR;
    }
    sc = WebPGetFeatures(blob, blob_size, &config.input);
    if (sc != VP8_STATUS_OK) {
        LOG_ERROR("WebPGetFeatures failed sc=%d",  sc);
        return TS_WEBP_DECODE_ERROR;
    }
    bufsize = width * height * 3;
    uint8_t *img = (uint8_t *) MALLOC(bufsize);
    if (img == nullptr) {
        return TS_NO_MEMORY;
    }
    if (WebPDecodeRGBInto(blob, blob_size,
                          img, bufsize, width * 3) == NULL) {
        LOG_ERROR("WebPDecode failed");
        WebPFreeDecBuffer(&config.output);
        FREE(buffer);
        return TS_WEBP_DECODE_ERROR;
    }
    *buffer = img;
    WebPFreeDecBuffer(&config.output);
    return (config.input.format == 2) ? TS_VALID : TS_WEBP_LOSSY_COMPRESSION;
}