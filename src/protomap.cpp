
#include <Arduino.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <string_view>

#include "buffer_ref.h"

#ifdef ESP32
    #include <esp_heap_caps.h>
#endif

#include "zlib.h"

#include "pngle.h"

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

#include "logging.hpp"

#include "protomap.hpp"
#include "slippytiles.hpp"

using namespace std;
using namespace pmtiles;

pmErrno_t decompress_gzip(const string_view str, buffer_ref &out);

static void evictTile(uint64_t key, tile_t *t);
static cache::lru_cache<uint64_t, tile_t *> tile_cache(TILECACHE_SIZE, {}, evictTile);
static vector<demInfo_t *> dems;
static uint8_t dbindex;
static uint8_t pngSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

bool isNull(const entryv3 &e) {
    return ((e.tile_id == 0) &&  (e.offset == 0) &&
            (e.length == 0) && (e.run_length == 0));
}

static size_t io_size;
static char *io_buffer;

pmErrno_t get_bytes(FILE *fp,
                    off_t start,
                    size_t length,
                    buffer_ref &result) {

    if (io_size < length) {
        io_size = alloc_size(length);
#ifdef ESP32
        io_buffer = (char *)heap_caps_realloc(io_buffer, io_size, MALLOC_CAP_SPIRAM);
#else
        io_buffer = (char *)realloc(io_buffer, io_size);
#endif
        if (io_buffer == nullptr) {
            LOG_ERROR("realloc %zu failed:  %s", length, strerror(errno));
            pmerrno = PM_IOBUF_ALLOC_FAILED;
            io_size = 0;
            return PM_IOBUF_ALLOC_FAILED;
        }
        LOG_DEBUG("%s iniial alloc %zu", __FUNCTION__, io_size);
    }

    off_t ofst = fseek(fp, start, SEEK_SET);
    if (ofst != 0 ) {
        LOG_ERROR("fseek to %ld failed:  %s", start, strerror(errno));
        return PM_SEEK_FAILED;;
    }
    size_t got = fread(io_buffer, 1, length, fp);
    if (got != length) {
        LOG_ERROR("read failed: got %zu of %zu, %s", got, length, strerror(errno));
        return PM_READ_FAILED;
    }
    result = buffer_ref(io_buffer, length);
    return PM_OK;
}

int addDEM(const char *path, demInfo_t **demInfo) {
    struct	stat st;
    int rc;
    int8_t pmerr;

    if (stat(path, &st)) {
        perror(path);
        return -1;
    }
    LOG_DEBUG("open '%s' size %ld : %s", path, st.st_size, strerror(errno));

    demInfo_t *di = new demInfo_t();
    di->fp = fopen(path, "rb");
    if (di->fp  == NULL) {
        perror(path);
        return -1;
    }
    buffer_ref io;
    if ((rc = get_bytes(di->fp, 0, 127, io)) != PM_OK) {
        delete di;
        return -1;
    }

    di->header = deserialize_header(string_view(io.to_string()), pmerr);
    if (pmerr != PMAP_OK) {
        LOG_ERROR("deserialize_header failed: %d", pmerr);
        delete di;
        return -1;
    }
    di->tile_size = TILESIZE; // FIXME
    di->path = strdup(path);  // FIXME

    dems.push_back(di);
    if (demInfo != NULL) {
        *demInfo = di;
    }
    return 0;
}

bool demContains(demInfo_t *di, double lat, double lon) {
    int32_t lat_e7 = to_e7(lat);
    int32_t lon_e7 = to_e7(lon);
    return  ((lat_e7 > di->header.min_lat_e7) && (lat_e7 < di->header.max_lat_e7) &&
             (lon_e7 > di->header.min_lon_e7) && (lon_e7 < di->header.max_lon_e7));
}

string keyStr(uint64_t key) {
    xyz_t k;
    k.key = key;
    return string_format("dem=%d %d/%d/%d",k.entry.index, k.entry.z, k.entry.x, k.entry.y);
}

void printCache(void) {
    for (auto item: tile_cache.items()) {
        LOG_INFO("%s", keyStr(item.first).c_str());
    }
}

void printDems(void) {
    for (auto d: dems) {
        LOG_INFO("dem %u: %s coverage %.2f/%.2f..%.2f/%.2f tile_decode_err=%lu protomap_err=%lu hits=%lu misses=%lu tilesize=%u type %s",
                 d->index, d->path,
                 min_lat(d), min_lon(d),
                 max_lat(d), max_lon(d),
                 d->tile_decode_errors,
                 d->protomap_errors,
                 d->cache_hits, d->cache_misses, d->tile_size,
                 tileType(d->header.tile_type));
    }
}

static void freeTile(tile_t *tile) {
    if (tile == NULL)
        return;
    if (tile->buffer != NULL)
        heap_caps_free(tile->buffer);
    heap_caps_free(tile);
}

static void evictTile(uint64_t key, tile_t *t) {
    LOG_DEBUG("evict %s", keyStr(key).c_str());
    if (t != NULL) {
        freeTile(t);
    }
}

static void pngle_init_cb(pngle_t *pngle, uint32_t w, uint32_t h) {
    size_t size =  w * h * 3;
    tile_t *tile = (tile_t *)heap_caps_malloc(sizeof(tile_t), MALLOC_CAP_SPIRAM);
    tile->buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    tile->width = w;
    pngle_set_user_data(pngle, tile);
}

static int lines = 6;
static void pngle_draw_cb(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
    tile_t *tile = (tile_t *) pngle_get_user_data(pngle);
    size_t offset = (x + tile->width * y) * 3;
    tile->buffer[offset] = rgba[0];
    tile->buffer[offset+1] = rgba[1];
    tile->buffer[offset+2] = rgba[2];
}

bool lookupTile(demInfo_t &di, locInfo_t *locinfo, double lat, double lon) {
    xyz_t key;
    int rc;
    int8_t pmerr;
    tile_t *tile = NULL;
    uint32_t offset_x, offset_y;
    uint32_t tile_x, tile_y;
    compute_pixel_offset(lat, lon, di.header.max_zoom, di.tile_size,
                         tile_x, tile_y, offset_x, offset_y);

    key.entry.index = di.index;
    key.entry.x =  (uint16_t)tile_x;
    key.entry.y =  (uint16_t)tile_y;
    key.entry.z = di.header.max_zoom;

    if (!tile_cache.exists(key.key)) {
        LOG_DEBUG("cache entry %s not found", keyStr(key.key).c_str());
        di.cache_misses++;

        TIMESTAMP(query);
        STARTTIME(query);

        uint64_t tile_id = zxy_to_tileid(di.header.max_zoom, tile_x, tile_y, pmerr);
        if (pmerr != PMAP_OK) {
            LOG_ERROR("zxy_to_tileid failed: %d", pmerr);
            locinfo->status = LS_PM_XYZ2TID_FAILED;
            di.protomap_errors++;
            return false;
        }
        bool found = false;

        uint64_t dir_offset  = di.header.root_dir_offset;
        uint64_t dir_length = di.header.root_dir_bytes;

        buffer_ref decomp;
        buffer_ref io;

        for (int i = 0; i < 4; i++) {
            if ((rc = get_bytes(di.fp, dir_offset, dir_length, io)) != PM_OK) {
                locinfo->status = LS_PM_GETBYTES_FAILED;
                di.protomap_errors++;
                return false;
            }
            std::vector<entryv3> directory;
            if (di.header.internal_compression == COMPRESSION_GZIP) {
                if ((rc = decompress_gzip(string_view(io.to_string()), decomp)) != PM_OK) {
                    locinfo->status = LS_GUNZIP_FAILED;
                    di.protomap_errors++;
                    return false;
                }
                directory = deserialize_directory(decomp.to_string(), pmerr);
            } else {
                directory = deserialize_directory(io.to_string(), pmerr);
            }
            if (pmerr) {
                locinfo->status = LS_DESERIALIZE_DIR_FAILED;
                di.protomap_errors++;
                return false;
            }
            entryv3 result = find_tile(directory,  tile_id);
            if (!isNull(result)) {
                if (result.run_length == 0) {
                    dir_offset = di.header.leaf_dirs_offset + result.offset;
                    dir_length = result.length;
                } else {
                    if ((rc = get_bytes(di.fp, di.header.tile_data_offset + result.offset, result.length, io)) != PM_OK) {
                        locinfo->status = LS_PM_GETBYTES_FAILED;
                        di.protomap_errors++;
                        return false;
                    }
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            size_t blob_size = io.size();
            const uint8_t *blob = (uint8_t *)io.data();

            PRINT_LAPTIME("blob retrieve %lu us", query);

            switch (di.header.tile_type) {
                case TILETYPE_PNG: {
                        TIMESTAMP(pngdecode);
                        STARTTIME(pngdecode);
                        pngle_t *pngle = pngle_new();
                        pngle_set_init_callback(pngle, pngle_init_cb);
                        pngle_set_draw_callback(pngle, pngle_draw_cb);
                        int fed = pngle_feed(pngle, blob, blob_size);
                        if (fed != blob_size) {
                            LOG_ERROR("%s: decode failed: decoded %d out of %u: %s",
                                      keyStr(key.key).c_str(), fed, blob_size, pngle_error(pngle));
                            freeTile(tile);
                            tile = NULL;
                            di.tile_decode_errors++;
                            locinfo->status = LS_PNG_DECODE_ERROR;
                        } else {
                            pngle_ihdr_t *hdr = pngle_get_ihdr(pngle);
                            if (hdr->compression) {
                                locinfo->status = LS_PNG_COMPRESSED;
                                LOG_ERROR("%s: compressed PNG tile",
                                          keyStr(key.key).c_str());
                            } else {
                                di.tile_size = pngle_get_width(pngle);
                                tile = (tile_t *) pngle_get_user_data(pngle);
                                tile_cache.put(key.key, tile);
                                locinfo->status = LS_VALID;
                            }
                        }
                        pngle_destroy(pngle);
                        PRINT_LAPTIME("PNG decode %lu us", pngdecode);
                    }
                    break;
                case TILETYPE_WEBP: {
                        TIMESTAMP(webpdecode);
                        STARTTIME(webpdecode);

                        int width, height;
                        VP8StatusCode sc;
                        WebPDecoderConfig config;
                        size_t bufsize;
                        uint8_t *buffer;

                        WebPInitDecoderConfig(&config);
                        rc = WebPGetInfo(blob, blob_size, &width, &height);
                        if (!rc) {
                            LOG_ERROR("%s: WebPGetInfo failed rc=%d", keyStr(key.key).c_str(), rc);
                            locinfo->status = LS_WEBP_DECODE_ERROR;
                            break;
                        }
                        sc = WebPGetFeatures(blob, blob_size, &config.input);
                        if (sc != VP8_STATUS_OK) {
                            LOG_ERROR("%s: WebPGetFeatures failed sc=%d", keyStr(key.key).c_str(), sc);
                            locinfo->status = LS_WEBP_DECODE_ERROR;
                            break;
                        }
                        if (config.input.format != 2) {
                            LOG_ERROR("%s: lossy WEBP compression", keyStr(key.key).c_str());
                            locinfo->status = LS_WEBP_COMPRESSED;
                            break;
                        }
                        tile = (tile_t *) heap_caps_malloc(sizeof(tile_t), MALLOC_CAP_SPIRAM);
                        bufsize = width * height * 3;
                        buffer = (uint8_t *) heap_caps_malloc(bufsize, MALLOC_CAP_SPIRAM);
                        if ((tile != NULL) && (buffer != NULL) && (bufsize != 0)) {
                            tile->buffer = buffer;
                            tile->width = config.input.width;
                            if (WebPDecodeRGBInto(blob, blob_size,
                                                  buffer, bufsize, width * 3) == NULL) {
                                LOG_ERROR("%s: WebPDecode failed", keyStr(key.key).c_str());
                                freeTile(tile);
                                tile = NULL;
                                di.tile_decode_errors++;
                                locinfo->status = LS_WEBP_DECODE_ERROR;
                            } else {
                                di.tile_size = config.input.width;
                                tile_cache.put(key.key, tile);
                                locinfo->status = LS_VALID;
                            }
                            WebPFreeDecBuffer(&config.output);
                            PRINT_LAPTIME("WEBP decode %lu us", webpdecode);
                        }
                        break;
                    default:
                        locinfo->status = LS_UNKNOWN_IMAGE_FORMAT;
                        break;
                    }
            }
        }
    } else {
        LOG_DEBUG("cache entry %s found: ", keyStr(key.key).c_str());
        tile = tile_cache.get(key.key);
        locinfo->status = LS_VALID;
        di.cache_hits++;
    }
    // assert(tile->buffer != NULL);

    if (locinfo->status == LS_VALID) {
        size_t i = round(offset_x)  + round(offset_y) * di.tile_size;
        locinfo->elevation = rgb2alt(&tile->buffer[i * 3] );
        return true;
    }
    return false;
}

int getLocInfo(double lat, double lon, locInfo_t *locinfo) {
    for (auto di: dems) {
        if (demContains(di, lat, lon)) {
            LOG_DEBUG("%.2f %.2f contained in %s", lat, lon, di->path);
            if (lookupTile(*di, locinfo, lat, lon)) {
                return 0;
            }
        }
    }
    locinfo->status = LS_TILE_NOT_FOUND;
    return 0;
}

string string_format(const string fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
    string str;
    va_list ap;
    while (1) {     // Maximum two passes on a POSIX system...
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {  // Everything worked
            str.resize(n);
            return str;
        }
        if (n > -1)  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 2;      // Guess at a larger size (OS specific)
    }
    return str;
}

const char *tileType(uint8_t tile_type) {
    switch (tile_type) {
        case TILETYPE_UNKNOWN:
        default:
            return "UNKNOWN";
        case TILETYPE_MVT:
            return "MVT";
        case TILETYPE_PNG:
            return "PNG";
        case TILETYPE_JPEG:
            return "JPEG";
        case TILETYPE_WEBP:
            return "WEBP";
        case TILETYPE_AVIF:
            return "AVIF";
    }
}
