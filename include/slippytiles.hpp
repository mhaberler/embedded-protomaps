#pragma once

#include <stdint.h>
#include <math.h>

static inline double to_radians(double degrees) {
    return degrees * M_PI / 180.0;
}

static inline double to_degrees(double radians) {
    return radians * (180.0 / M_PI);
}

static inline double rgb2alt(const uint8_t *px) {
    return  -10000 + ((px[0] * 256 * 256 + px[1] * 256 + px[2]) * 0.1);
}

static inline int32_t to_e7(const double &v) {
    return int(v * 10000000);
}

static inline double from_e7(const int32_t &v) {
    return double(v) / 10000000.0;
}

static inline double resolution(double latitude, uint32_t zoom) {
    return 156543.03 * cos(to_radians(latitude)) / pow(2.0, (double)zoom);
}

static inline double tilex2long(int32_t x, uint32_t zoom) {
    return x / (double)(1 << zoom) * 360.0 - 180;
}

static inline double tiley2lat(int32_t y, uint32_t zoom) {
    double n = M_PI - 2.0 * M_PI * y / (double)(1 << zoom);
    return to_degrees(atan(0.5 * (exp(n) - exp(-n))));
}

static inline void lat_lon_to_pixel(double lat, double  lon, uint32_t zoom, int32_t tile_size, double &x, double &y) {
    int32_t num_tiles = (1 << zoom);
    x = (lon + 180.0) / 360.0 * num_tiles * tile_size;
    y = ((1 - log(tan(to_radians(lat)) + 1 / cos(to_radians(lat))) / M_PI) / 2 * num_tiles * tile_size);
}

static inline void lat_lon_to_tile(double lat, double  lon, uint32_t zoom, int32_t tile_size, uint32_t& tile_x, uint32_t& tile_y) {
    double pixel_x, pixel_y;
    lat_lon_to_pixel(lat, lon, zoom, tile_size, pixel_x, pixel_y);
    tile_x = pixel_x / 256;
    tile_y = pixel_y / 256;
}

static inline void compute_pixel_offset(double lat, double  lon, uint32_t zoom, int32_t tile_size,
                          uint32_t& tile_x, uint32_t& tile_y, uint32_t &offset_x, uint32_t &offset_y) {
    double pixel_x, pixel_y;
    lat_lon_to_tile(lat, lon, zoom, tile_size,  tile_x, tile_y);
    lat_lon_to_pixel(lat, lon, zoom, tile_size, pixel_x, pixel_y);
    offset_x = round(pixel_x - tile_x * 256);
    offset_y = round(pixel_y - tile_y * 256);
}
