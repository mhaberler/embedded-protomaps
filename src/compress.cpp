#include <string>
#include <string_view>
#ifdef ESP32
    #include <esp_heap_caps.h>
#endif

#include "protomap.hpp"
#include "logging.hpp"
#include "zlib.h"

#define GUESS 2
#define MOD_GZIP_ZLIB_WINDOWSIZE 15


static size_t decomp_size;
static char *decomp_buffer;

pmErrno_t decompress_gzip(const string_view str, buffer_ref &out) {

    if (decomp_size < str.size()) {
        decomp_size = alloc_size(str.size() * GUESS);
#ifdef ESP32
        decomp_buffer = (char *)heap_caps_realloc(decomp_buffer, decomp_size, MALLOC_CAP_SPIRAM);
#else
        decomp_buffer = (char *)realloc(decomp_buffer, decomp_size);
#endif
        if (decomp_buffer == nullptr) {
            LOG_ERROR("realloc %zu -> %zu failed:  %s", str.size(), decomp_size, strerror(errno));
            pmerrno = PM_DECOMP_BUF_ALLOC_FAILED;
            if (decomp_buffer)
                free(decomp_buffer);
            decomp_size = 0;
            return PM_DECOMP_BUF_ALLOC_FAILED;
        }
        LOG_DEBUG("%s initial alloc %zu", __FUNCTION__, decomp_size);
    }

    z_stream zs = {};
    int32_t rc = inflateInit2(&zs, MOD_GZIP_ZLIB_WINDOWSIZE + 16);

    if (rc != Z_OK) {
        LOG_ERROR("inflateInit failed");
        return PM_DEFLATE_INIT_FAILED;
    }

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();

    size_t current = 0;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(decomp_buffer + zs.total_out);
        zs.avail_out = decomp_size - zs.total_out;

        rc = inflate(&zs, 0);

        if (decomp_size <= zs.total_out) {
            decomp_size = alloc_size(zs.total_out*2);
            LOG_DEBUG("%s grow -> %zu", __FUNCTION__, decomp_size);
#ifdef ESP32
            decomp_buffer = (char *)heap_caps_realloc(decomp_buffer, decomp_size, MALLOC_CAP_SPIRAM);
#else
            decomp_buffer = (char *)realloc(decomp_buffer, decomp_size);
#endif
        }

    } while (rc == Z_OK);

    inflateEnd(&zs);

    if (rc != Z_STREAM_END) {
        LOG_ERROR("%s: zlib decompression failed : %ld", __FUNCTION__, rc);
        return PM_ZLIB_DECOMP_FAILED;
    }
    out = buffer_ref(decomp_buffer, zs.total_out);
    return PM_OK;
}