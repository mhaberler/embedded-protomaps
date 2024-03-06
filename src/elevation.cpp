#include "logging.hpp"
#include "elevation.hpp"

bool getElevation(double lat, double lon, locationInfo_t &li) {
    mapInfo_t *mi = findMap(lat, lon, true);
    if (!mi) {
        li.status = LS_NO_MAP;
        return false;
    }
    li.map = mi;
    uint32_t offset_x, offset_y;
    tileStatus_t status = fetchTileByLatLong(*mi, &li.tile, lat, lon, mi->header.max_zoom, offset_x, offset_y);
    if (status != TS_VALID) {
        return false;
    }
    if (!li.tile->buffer)
        return false;
    size_t i = round(offset_x)  + round(offset_y) * li.tile->width;
    li.elevation = rgb2alt(&li.tile->buffer[i * 3] );
    li.status = LS_VALID;
    return true;
}

const char *locationStatus(locationStatus_t ls) {
    switch (ls) {
        default:
            return "UNKNOWN";
        case LS_VALID:
            return "LS_VALID";
        case LS_NODATA:
            return "LS_NODATA";
        case LS_NO_COVERAGE:
            return "LS_NO_COVERAGE";
        case LS_NO_MAP:
            return "LS_NO_MAP";
    }
}