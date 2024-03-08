#include <SdFat.h>
#include <string_view>

#include "mvt.hpp"
#include "logging.hpp"
#include "protomap.hpp"
#include "slippytiles.hpp"

using namespace std;
using namespace pmtiles;

extern SdFs sd;

pmErrno_t decompress_gzip(const string_view str, buffer_ref &out);

static void evictTile(uint64_t key, tile_t *t);
static cache::lru_cache<uint64_t, tile_t *> tile_cache(TILECACHE_SIZE, {}, evictTile);
static vector<mapInfo_t *> maps;
static uint8_t dbindex;
static uint8_t pngSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

bool isNull(const entryv3 &e) {
    return ((e.tile_id == 0) &&  (e.offset == 0) &&
            (e.length == 0) && (e.run_length == 0));
}

static size_t io_size;
static char *io_buffer;

pmErrno_t get_bytes(SdBaseFile &fp,
                    uint64_t start,
                    size_t length,
                    buffer_ref &result) {

    if (io_size < length) {
        io_size = alloc_size(length);
        io_buffer = (char *)REALLOC(io_buffer, io_size);
        if (io_buffer == nullptr) {
            LOG_ERROR("realloc %zu failed:  %s", length, strerror(errno));
            pmerrno = PM_IOBUF_ALLOC_FAILED;
            io_size = 0;
            return PM_IOBUF_ALLOC_FAILED;
        }
    }
    LOG_DEBUG("%s read %u from  %llu", __FUNCTION__, length, start);

    if (!fp.seekSet(start)) {
        LOG_ERROR("seekSet to %lld failed", start);
        return PM_SEEK_FAILED;;
    }
    if (fp.read(io_buffer, length) < 0) {
        LOG_ERROR("read(%lu) failed", length);
        return PM_READ_FAILED;
    }
    result = buffer_ref(io_buffer, length);
    return PM_OK;
}

mapInfo_t *addMap(const char *path) {
    struct stat st;
    int rc;
    int8_t pmerr;

    if (!sd.exists(path)) {

    }
    mapInfo_t *mi = new mapInfo_t();
    // if (!sd.open(&mi->fp, path, O_RDONLY)) {
    mi->fp = sd.open(path, O_RDONLY);
    if (!mi->fp.isOpen()) {
        LOG_ERROR("cant open %s", path);
        return nullptr;
    }

    buffer_ref io;
    if ((rc = get_bytes(mi->fp, 0, 127, io)) != PM_OK) {
        delete mi;
        return nullptr;
    }

    mi->header = deserialize_header(string_view(io.to_string()), pmerr);
    if (pmerr != PMAP_OK) {
        LOG_ERROR("deserialize_header failed: %d", pmerr);
        delete mi;
        return nullptr;
    }


    mi->tile_size = TILESIZE; // FIXME
    mi->path = string(path);

    maps.push_back(mi);
    return mi;
}

bool parseMetadata(mapInfo_t *mi) {
    int rc;
    if (mi->header.json_metadata_bytes > 0) {
        TIMESTAMP(_metadata);
        STARTTIME(_metadata);
        buffer_ref io;
        if ((rc = get_bytes(mi->fp, mi->header.json_metadata_offset, mi->header.json_metadata_bytes, io)) != PM_OK) {
            return false;
        }
        buffer_ref decomp;
        if (mi->header.internal_compression == COMPRESSION_GZIP) {
            rc = decompress_gzip(string_view(io.to_string()), decomp);
            if (rc != PM_OK) {
                LOG_ERROR("decompress_gzip failed : %d", rc);
                return false;
            } else {
                LOG_INFO("read and decompress metadata: %.1f mS", LAPTIME(_metadata)/1000.0);
                STARTTIME(_metadata);

                DeserializationError err = deserializeJson(mi->metadata, decomp.c_str());
                if (err != DeserializationError::Ok) {
                    LOG_ERROR("parsing JSON metadata failed: '%s'", err.c_str());
                } else {
                    LOG_INFO("parse JSON metadata: %.1f mS",  LAPTIME(_metadata)/1000.0);
                    // serializeJson(mi->metadata, Serial);
                    // Serial.println();
                    LOG_INFO("metadata sizes: %lu %lu",
                             (uint32_t) mi->header.json_metadata_bytes,
                             decomp.length());
                }
            }
        }
    } else {
        LOG_INFO("no metadata");
    }
    return true;
}

bool mapContains(mapInfo_t *mi, double lat, double lon) {
    int32_t lat_e7 = to_e7(lat);
    int32_t lon_e7 = to_e7(lon);
    return  ((lat_e7 > mi->header.min_lat_e7) && (lat_e7 < mi->header.max_lat_e7) &&
             (lon_e7 > mi->header.min_lon_e7) && (lon_e7 < mi->header.max_lon_e7));
}

string keyStr(uint64_t key) {
    xyz_t k;
    k.key = key;
    return string_format("map=%d %d/%d/%d",k.entry.index, k.entry.z, k.entry.x, k.entry.y);
}

void dumpCache(void) {
    LOG_INFO("%lu cached tiles:",  tile_cache.size());
    for (auto item: tile_cache.items()) {
        LOG_INFO("%s", keyStr(item.first).c_str());
    }
}
void clearCache(void) {
    LOG_INFO("BROKEN. removing %lu cached tiles",  tile_cache.size());
    for (auto item: tile_cache.items()) {
        // tile_cache.remove(item.first);
        // this error I dont understand:
        // src/protomap.cpp:156:26:   required from here
        // include/lrucache.hpp:74:55: error: cannot convert 'std::_List_iterator<std::pair<long long unsigned int, tile_t*> >' to 'tile_t*' in argument passing
        //    74 |             if (_evict != NULL) _evict(it->first, it->second);
        // ----------------------------------------------------------^^^^^^^^^^
    }
}

void dumpMaps(void) {
    LOG_INFO("%lu map(s) loaded:",  maps.size());
    for (auto m: maps) {
        LOG_INFO("map %u: %s coverage %.2f/%.2f..%.2f/%.2f tile_decode_err=%lu protomap_err=%lu hits=%lu misses=%lu tilesize=%u type %s",
                 m->index, m->path.c_str(),
                 min_lat(m), min_lon(m),
                 max_lat(m), max_lon(m),
                 m->tile_decode_errors,
                 m->protomap_errors,
                 m->cache_hits, m->cache_misses, m->tile_size,
                 tileType(m->header.tile_type));
    }
}

static void freeTile(tile_t *tile) {
    if (tile == NULL)
        return;
    if (tile->buffer != NULL)
        FREE(tile->buffer);
    FREE(tile);
}

static void evictTile(uint64_t key, tile_t *t) {
    LOG_DEBUG("evict %s", keyStr(key).c_str());
    if (t != NULL) {
        freeTile(t);
    }
}

tileStatus_t fetchTileByLatLong(mapInfo_t &mi, tile_t **tile, double lat, double lon, uint8_t zoom, uint32_t &offset_x, uint32_t & offset_y) {
    xyz_t key;
    int rc;
    int8_t pmerr;
    uint32_t tile_x, tile_y;
    compute_pixel_offset(lat, lon, zoom, mi.tile_size,
                         tile_x, tile_y, offset_x, offset_y);
    *tile = nullptr;
    tileStatus_t st = fetchTileXYZ(mi, tile, tile_x, tile_y, zoom);
    if (*tile) {
        (*tile)->offset_x = offset_x;
        (*tile)->offset_y = offset_y;
    }
    return st;
}

tileStatus_t fetchTileXYZ(mapInfo_t &mi, tile_t **t, uint16_t tile_x, uint16_t tile_y, uint8_t zoom) {
    xyz_t key;
    int rc;
    int8_t pmerr;
    uint32_t offset_x, offset_y;

    LOG_VERBOSE("%u %lu %lu", zoom, tile_x, tile_y);

    key.entry.index = mi.index;
    key.entry.x =  (uint16_t)tile_x;
    key.entry.y =  (uint16_t)tile_y;
    key.entry.z = mi.header.max_zoom;
    TIMESTAMP(fetchtile);

    if (!tile_cache.exists(key.key)) {
        LOG_DEBUG("cache entry %s not found", keyStr(key.key).c_str());
        mi.cache_misses++;

        STARTTIME(fetchtile);

        uint64_t tile_id = zxy_to_tileid(mi.header.max_zoom, tile_x, tile_y, pmerr);
        if (pmerr != PMAP_OK) {
            LOG_ERROR("zxy_to_tileid failed: %d", pmerr);
            mi.protomap_errors++;
            return TS_PM_XYZ2TID_FAILED;
        }
        bool found = false;

        uint64_t dir_offset  = mi.header.root_dir_offset;
        uint64_t dir_length = mi.header.root_dir_bytes;

        buffer_ref decomp;
        buffer_ref io;

        for (int i = 0; i < 4; i++) {
            if ((rc = get_bytes(mi.fp, dir_offset, dir_length, io)) != PM_OK) {
                mi.protomap_errors++;
                return TS_PM_GETBYTES_FAILED;
            }
            std::vector<entryv3> directory;
            if (mi.header.internal_compression == COMPRESSION_GZIP) {
                if ((rc = decompress_gzip(string_view(io.to_string()), decomp)) != PM_OK) {
                    mi.protomap_errors++;
                    return TS_GUNZIP_FAILED;
                }
                LOG_DEBUG("gunzip %u -> %u", io.size(), decomp.size());
                directory = deserialize_directory(decomp.to_string(), pmerr);
            } else {
                directory = deserialize_directory(io.to_string(), pmerr);
            }
            if (pmerr) {
                mi.protomap_errors++;
                return TS_DESERIALIZE_DIR_FAILED;
            }
            entryv3 result = find_tile(directory,  tile_id);
            if (!isNull(result)) {
                if (result.run_length == 0) {
                    dir_offset = mi.header.leaf_dirs_offset + result.offset;
                    dir_length = result.length;
                } else {
                    if ((rc = get_bytes(mi.fp, mi.header.tile_data_offset + result.offset, result.length, io)) != PM_OK) {
                        mi.protomap_errors++;
                        return TS_PM_GETBYTES_FAILED;
                    }
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            size_t blob_size = 0;
            const uint8_t *blob = nullptr;

            PRINT_LAPTIME("blob retrieve %lu us", fetchtile);
            STARTTIME(fetchtile);

            switch (mi.header.tile_compression) {
                case COMPRESSION_NONE:
                    blob_size = io.size();
                    blob = (uint8_t *)io.data();
                    break;
                case COMPRESSION_GZIP:
                    if ((rc = decompress_gzip(string_view(io.to_string()), decomp)) != PM_OK) {
                        mi.protomap_errors++;
                        return TS_TILE_GUNZIP_FAILED;
                    }
                    blob_size = decomp.size();
                    blob = (uint8_t *)decomp.data();
                    LOG_DEBUG("gunzip %u -> %u", io.size(), decomp.size());
                    PRINT_LAPTIME("tile decompress %lu us", fetchtile);
                    break;

                case COMPRESSION_UNKNOWN:
                case COMPRESSION_BROTLI:
                case COMPRESSION_ZSTD:
                    mi.protomap_errors++;
                    LOG_ERROR("unsuppored tile_compression %u", mi.header.tile_compression);
                    return TS_UNSUPPORTED_TILE_COMPRESSION;
                    break;
            }
            STARTTIME(fetchtile);
            switch (mi.header.tile_type) {
                case TILETYPE_PNG: {
                        int width, height;
                        uint8_t *buffer;
                        tileStatus_t st = decode_webp(blob, blob_size, width, height, &buffer);
                        switch (st) {
                            case TS_VALID: {
                                    tile_t *tile = (tile_t *) MALLOC(sizeof(tile_t));
                                    tile->width = width;
                                    tile->height = height;
                                    tile->status = st;
                                    tile->buffer = buffer;
                                    tile->mapinfo = &mi;
                                    tile->tile_x = key.entry.x;
                                    tile->tile_y = key.entry.y;
                                    tile->zoom = key.entry.z;
                                    tile->tile_type = mi.header.tile_type;
                                    tile_cache.put(key.key, tile);
                                    *t = tile;
                                    PRINT_LAPTIME("blob decompress %lu us", fetchtile);
                                    return st;
                                }
                                break;
                            default:
                                mi.tile_decode_errors++;
                                return st;
                        }
                    }
                    break;
                case TILETYPE_WEBP: {
                        int width, height;
                        uint8_t *buffer;
                        tileStatus_t st = decode_webp(blob, blob_size, width, height, &buffer);
                        switch (st) {
                            case TS_VALID:
                            case TS_WEBP_LOSSY_COMPRESSION: {
                                    tile_t *tile = (tile_t *) MALLOC(sizeof(tile_t));
                                    tile->width = width;
                                    tile->height = height;
                                    tile->status = st;
                                    tile->buffer = buffer;
                                    tile->mapinfo = &mi;
                                    tile->tile_x = key.entry.x;
                                    tile->tile_y = key.entry.y;
                                    tile->zoom = key.entry.z;
                                    tile->tile_type = mi.header.tile_type;
                                    tile_cache.put(key.key, tile);
                                    *t = tile;
                                    PRINT_LAPTIME("blob decompress %lu us", fetchtile);

                                    return st;
                                }
                                break;
                            default:
                                mi.tile_decode_errors++;
                                return st;
                        }
                    }
                    break;
                case TILETYPE_MVT: {
                        LOG_DEBUG("%s: MVT", keyStr(key.key).c_str());
                        // tile_t *tile = (tile_t *)MALLOC(sizeof(tile_t));
                        tile_t *tile = new tile_t();  // FIXME
                        tileStatus_t st = decode_mvt(blob, blob_size, tile->mvt);
                        if (st != TS_MVTDECODE_OK) {
                            mi.tile_decode_errors++;
                            FREE(tile);
                        } else {
                            tile->buffer = nullptr;
                            tile->status = st;
                            tile->mapinfo = &mi;
                            tile->tile_x = key.entry.x;
                            tile->tile_y = key.entry.y;
                            tile->zoom = key.entry.z;
                            tile->tile_type = mi.header.tile_type;
                            tile_cache.put(key.key, tile);
                            PRINT_LAPTIME("blob decompress %lu us", fetchtile);
                            *t = tile;
                        }
                        return st;
                    }
                    break;
                default:
                    return TS_UNKNOWN_IMAGE_FORMAT;
            }
        } else {
            return TS_TILE_NOT_FOUND;
        }
    } else {
        LOG_DEBUG("cache entry %s found: ", keyStr(key.key).c_str());
        tile_t *tile = tile_cache.get(key.key);
        *t = tile;
        mi.cache_hits++;
        return TS_VALID;
    }
    return TS_NOTREACHED;
}


mapInfo_t *findMap(double lat, double lon, bool raster_only) {
    for (auto mi: maps) {
        if (raster_only && (mi->header.tile_type == TILETYPE_MVT))
            continue;
        if (mapContains(mi, lat, lon)) {
            LOG_DEBUG("%.2f %.2f contained in raster map %s", lat, lon, mi->path.c_str());
            return mi;
        }
    }
    return nullptr;
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

const char *tileStatus(tileStatus_t st) {
    switch  (st) {
        case TS_VALID:
            return "TS_VALID";
        case TS_INVALID:
            return "TS_INVALID";
        case TS_NO_MEMORY:
            return "TS_NO_MEMORY";
        case TS_TILE_NOT_FOUND:
            return "TS_TILE_NOT_FOUND";
        case TS_WEBP_DECODE_ERROR:
            return "TS_WEBP_DECODE_ERROR";
        case TS_WEBP_LOSSY_COMPRESSION:
            return "TS_WEBP_LOSSY_COMPRESSION";
        case TS_PNG_DECODE_ERROR:
            return "TS_PNG_DECODE_ERROR";
        case TS_PNG_COMPRESSED:
            return "TS_PNG_COMPRESSED";
        case TS_WEBP_COMPRESSED:
            return "TS_WEBP_COMPRESSED";
        case TS_UNKNOWN_IMAGE_FORMAT:
            return "TS_UNKNOWN_IMAGE_FORMAT";
        case TS_PM_XYZ2TID_FAILED:
            return "TS_PM_XYZ2TID_FAILED";
        case TS_PM_GETBYTES_FAILED:
            return "TS_PM_GETBYTES_FAILED";
        case TS_GUNZIP_FAILED:
            return "TS_GUNZIP_FAILED";
        case TS_DESERIALIZE_DIR_FAILED:
            return "TS_DESERIALIZE_DIR_FAILED";
        case TS_TILE_TOO_LARGE:
            return "TS_TILE_TOO_LARGE";
        case TS_UNSUPPORTED_TILE_COMPRESSION:
            return "TS_UNSUPPORTED_TILE_COMPRESSION";
        case TS_TILE_GUNZIP_FAILED:
            return "TS_TILE_GUNZIP_FAILED";
        case TS_MVTDECODE_FAILED:
            return "TS_MVTDECODE_FAILED";
        case TS_MVTDECODE_OK:
            return "TS_MVTDECODE_OK";
        case TS_NOTREACHED:
            return "TS_NOTREACHED";
    }
    return "unknown error";
}