#pragma once

#include <SdFat.h>

#include <vector>
#include <string>
#include <string_view>
#include <ArduinoJson.h>

#include "alloc.h"
#include "buffer_ref.h"
#include "pmtiles.hpp"
#include "lrucache.hpp"
#include "slippytiles.hpp"

using namespace std;
using namespace pmtiles;

#ifndef TILESIZE
    #define TILESIZE 256
#endif

#ifndef TILECACHE_SIZE
    #define TILECACHE_SIZE 5
#endif

#ifndef MIN_ALLOC_SIZE
    #define MIN_ALLOC_SIZE 8192
#endif

typedef mqtt::buffer_ref<char> buffer_ref;

static inline size_t next_power_of_2(size_t s) {
    s--;
    s |= s >> 1;
    s |= s >> 2;
    s |= s >> 4;
    s |= s >> 8;
    s |= s >> 16;
    s++;
    return s;
};

static inline size_t alloc_size(size_t s) {
    if (s < MIN_ALLOC_SIZE)
        return MIN_ALLOC_SIZE;
    return next_power_of_2(s);
}

#define PM_MIN -128
typedef enum {
    PM_IOBUF_ALLOC_FAILED = PM_MIN,
    PM_SEEK_FAILED,
    PM_READ_FAILED,
    PM_DEFLATE_INIT_FAILED,
    PM_ZLIB_DECOMP_FAILED,
    PM_DECOMP_BUF_ALLOC_FAILED,

    PM_OK = 0,
} pmErrno_t;

typedef struct  {
    uint16_t index;
    uint16_t x;
    uint16_t y;
    uint16_t z;
} dbxyz_t;

union xyz_t {
    uint64_t key;
    dbxyz_t entry;
};

typedef enum {
    TS_VALID=0,
    TS_INVALID,
    TS_NO_MEMORY,
    TS_TILE_NOT_FOUND,
    TS_WEBP_DECODE_ERROR,
    TS_WEBP_LOSSY_COMPRESSION,
    TS_PNG_DECODE_ERROR,
    TS_PNG_COMPRESSED,
    TS_WEBP_COMPRESSED,
    TS_UNKNOWN_IMAGE_FORMAT,
    TS_PM_XYZ2TID_FAILED,
    TS_PM_GETBYTES_FAILED,
    TS_GUNZIP_FAILED,
    TS_DESERIALIZE_DIR_FAILED,
    TS_TILE_TOO_LARGE,
    TS_UNSUPPORTED_TILE_COMPRESSION,
    TS_TILE_GUNZIP_FAILED,
    TS_MVTDECODE_FAILED,
    TS_MVTDECODE_OK,
    TS_NOTREACHED,
} tileStatus_t;

typedef struct {
    string path;
    FsFile fp;
    headerv3 header;
    JsonDocument metadata;
    uint32_t tile_decode_errors;
    uint32_t protomap_errors;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint16_t tile_size;
    uint8_t index;
} mapInfo_t;


typedef struct {
    uint8_t *buffer;  // raster
    size_t width; // of a line in pixels
    size_t height; 
    uint32_t tile_x;
    uint32_t tile_y;
    uint8_t zoom;
    uint8_t tile_type;
    uint32_t offset_x, offset_y;
    JsonDocument mvt;
    mapInfo_t *mapinfo;
    tileStatus_t status;
} tile_t;


static inline double min_lat(const mapInfo_t *d) {
    return from_e7(d->header.min_lat_e7);
}

static inline double max_lat(const mapInfo_t *d) {
    return from_e7(d->header.max_lat_e7);
}

static inline double min_lon(const mapInfo_t *d) {
    return from_e7(d->header.min_lon_e7);
}

static inline double max_lon(const mapInfo_t *d) {
    return from_e7(d->header.max_lon_e7);
}

static inline double meters_per_pixel(const mapInfo_t *d) {
    return resolution(from_e7(d->header.center_lat_e7), d->header.max_zoom);
}

mapInfo_t *addMap(const char *path);
mapInfo_t *findMap(double lat, double lon, bool raster_only);
bool parseMetadata(mapInfo_t *mi);

tileStatus_t fetchTileByLatLong(mapInfo_t &mi, tile_t **tile, double lat, double lon, uint8_t zoom, uint32_t &offset_x, uint32_t & offset_y);

tileStatus_t fetchTileXYZ(mapInfo_t &mi, tile_t **tile, uint16_t x, uint16_t y, uint8_t z);

bool displayTile(tile_t *tile);

// decode an MVT tile into a JsonDocument
tileStatus_t decode_mvt(const uint8_t *blob, size_t blob_size, JsonDocument &doc);

// decode a webp-compressed blob
// buffer: callee allocates, caller frees
tileStatus_t decode_webp(const uint8_t *blob, size_t blob_size, int& width, int& height, uint8_t **buffer);

// decode a webp-compressed blob
// buffer: callee allocates, caller frees
tileStatus_t decode_png(const uint8_t *blob, size_t blob_size, int& width, int& height, uint8_t **buffer);

void dumpCache(void);
void clearCache(void);
void dumpMaps(void);
const char *tileType(uint8_t tile_type);
const char *tileStatus(tileStatus_t st);

string string_format(const string fmt, ...);
