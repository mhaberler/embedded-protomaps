#pragma once

#include <vector>
#include <string>


#include "buffer_ref.h"

#include "pmtiles.hpp"
#include "lrucache.hpp"
#include "slippytiles.hpp"

using namespace std;
using namespace pmtiles;

#ifdef ESP32_TIMING
    #define xstr(s) str(s)
    #define str(s) #s
    #define TIMESTAMP(x) int64_t x;
    #define STARTTIME(x) do { x = esp_timer_get_time();} while (0);
    #define PRINT_LAPTIME(fmt, x)  do { Serial.printf(fmt xstr(\n), (uint32_t) (esp_timer_get_time() - x)); } while (0);
    #define LAPTIME(x)  ((uint32_t) (esp_timer_get_time() - x))
#else
    #define TIMESTAMP(x)
    #define STARTTIME(x)
    #define PRINT_LAPTIME(fmt, x)
    #define LAPTIME(x) 0
#endif

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

typedef struct {
    uint8_t *buffer;
    size_t width; // of a line in pixels
} tile_t;

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
    LS_INVALID=0,
    LS_VALID,
    LS_NODATA,
    LS_TILE_NOT_FOUND,
    LS_WEBP_DECODE_ERROR,
    LS_PNG_DECODE_ERROR,
    LS_PNG_COMPRESSED,
    LS_WEBP_COMPRESSED,
    LS_UNKNOWN_IMAGE_FORMAT,
    LS_PM_XYZ2TID_FAILED,
    LS_PM_GETBYTES_FAILED,
    LS_GUNZIP_FAILED,
    LS_DESERIALIZE_DIR_FAILED,
} locStatus_t;

typedef struct {
    double elevation;
    locStatus_t status;
} locInfo_t;

typedef struct {
    const char *path;
    FILE *fp;
    headerv3 header;
    uint32_t tile_decode_errors;
    uint32_t protomap_errors;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint16_t tile_size;
    uint8_t index;
} demInfo_t;


static inline double min_lat(const demInfo_t *d) {
    return from_e7(d->header.min_lat_e7);
}

static inline double max_lat(const demInfo_t *d) {
    return from_e7(d->header.max_lat_e7);
}

static inline double min_lon(const demInfo_t *d) {
    return from_e7(d->header.min_lon_e7);
}

static inline double max_lon(const demInfo_t *d) {
    return from_e7(d->header.max_lon_e7);
}

static inline double meters_per_pixel(const demInfo_t *d) {
    return resolution(from_e7(d->header.center_lat_e7), d->header.max_zoom);
}

int addDEM(const char *path, demInfo_t **demInfo = NULL);
int getLocInfo(double lat, double lon, locInfo_t *locinfo);
void printCache(void);
void printDems(void);
const char *tileType(uint8_t tile_type);
string string_format(const string fmt, ...);
