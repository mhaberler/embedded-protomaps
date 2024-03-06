#pragma once
#include "protomap.hpp"

typedef enum {
    LS_VALID=0,
    LS_NODATA,
    LS_NO_COVERAGE,
    LS_NO_MAP
} locationStatus_t;


typedef struct {
    double elevation;
    mapInfo_t *map;
    tile_t *tile;
    locationStatus_t status;
} locationInfo_t;

bool getElevation(double lat, double lon, locationInfo_t &li);
const char *locationStatus(locationStatus_t ls);