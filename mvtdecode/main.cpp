#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <ArduinoJson.h>
#include "Print.h"

#include <pb_common.h>
#include <pb_decode.h>

#include "logging.hpp"
#include "vector_tile.pb.h"

using namespace std;
#define TEST(x) \
    if (!(x)) { \
        fprintf(stderr, "\033[31;1mFAILED:\033[22;39m %s:%d %s\n", __FILE__, __LINE__, #x); \
        status = 1; \
    } else { \
        printf("\033[32;1mOK:\033[22;39m %s\n", #x); \
    }
#define CHECK(x) \
    if (!(x)) { \
        fprintf(stderr, "\033[31;1mFAILED:\033[22;39m %s:%d %s\n", __FILE__, __LINE__, #x); \
        status = 1; \
    }
typedef enum {
    GC_MOVE_TO=1,
    GC_LINE_TO=2,
    GC_CLOSE_PATH=7,
} geometryCommand_t;

inline constexpr int32_t decode_zigzag32(uint32_t value) noexcept {
    return static_cast<int32_t>(value >> 1) ^ -static_cast<int32_t>(value & 1);
}
inline constexpr int64_t decode_zigzag64(uint64_t value) noexcept {
    return static_cast<int64_t>(value >> 1) ^ -static_cast<int64_t>(value & 1);
}

#define INITIAL_GEOM_SIZE  (1 << 16)
uint32_t *geom;
size_t geom_size;
int status = 0;

class Serial : public Print {};

static bool decode_geometry(pb_istream_s *stream, const pb_field_iter_s *s, void**arg) {
    if (geom == nullptr) {
        geom = (uint32_t*)malloc(INITIAL_GEOM_SIZE);
        geom_size = INITIAL_GEOM_SIZE;
    }
    JsonArray *ga = reinterpret_cast<JsonArray *>(*arg);
    JsonDocument g;

    uint32_t i = 0;
    while (stream->bytes_left) {
        uint32_t u;
        CHECK(pb_decode_varint32(stream, &u));
        geom[i] = u;
        i++;
    }
    CHECK(stream->bytes_left == 0);
    uint32_t m_end = i;
    uint32_t m_it = 0;
    uint32_t m_count;
    uint32_t m_max_count = m_end/2;
    int32_t cursorX = 0;
    int32_t cursorY = 0;
    while (m_it < m_end) {
        uint32_t command = geom[m_it++];
        uint32_t command_id = command & 0x7;
        m_count = command >> 3;
        CHECK(m_count <= m_max_count);

        g.clear();
        switch (command_id) {
        case GC_MOVE_TO: {
            CHECK(m_count == 1);
            cursorX = decode_zigzag32(geom[m_it++]);
            cursorY = decode_zigzag32(geom[m_it++]);
            g["op"] = "move_to";
            JsonDocument c;
            JsonArray coords = c.to<JsonArray>();
            coords.add(cursorX);
            coords.add(cursorY);
            g["coords"] = coords;
        }
        break;
        case GC_LINE_TO: {
            g["op"] = "line_to";
            JsonDocument c;
            JsonArray coords = c.to<JsonArray>();
            for (uint32_t i = 0; i < m_count; i++) {
                cursorX = decode_zigzag32(geom[m_it++]);
                cursorY = decode_zigzag32(geom[m_it++]);
                coords.add(cursorX);
                coords.add(cursorY);
            }
            g["coords"] = coords;
        }
        break;
        case GC_CLOSE_PATH: {
            CHECK(m_count == 1);
            g["op"] = "close_path";
        }
        break;
        default:
            LOG_ERROR("%s: bad geometry command ID %u", __FUNCTION__, command_id);
            return false;
        }
        ga->add(g);
    }
    return true;
}

static const char * geometry_type[] = {
    "UNKNOWN",
    "POINT",
    "LINESTRING",
    "POLYGON"
};

static bool decode_tag(pb_istream_s *stream, const pb_field_iter_s *s, void**arg) {
    JsonArray  *ua = reinterpret_cast<JsonArray *>(*arg);
    while (stream->bytes_left) {
        uint32_t u;
        if (!pb_decode_varint32(stream, &u)) {
            return false;
        }
        ua->add(u);
    }
    return true;
}

static bool decode_feature(pb_istream_s *stream, const pb_field_iter_s *s, void**arg) {
    JsonArray *f = reinterpret_cast<JsonArray *>(*arg);
    JsonDocument gs,ts;
    JsonArray geometries = gs.to<JsonArray>();
    JsonArray tags = ts.to<JsonArray>();
    Tile_Feature feature = Tile_Feature_init_zero;

    feature.geometry.funcs.decode = decode_geometry;
    feature.geometry.arg = &geometries;

    feature.tags.funcs.decode = &decode_tag;
    feature.tags.arg = &tags;

    CHECK(pb_decode(stream, Tile_Feature_fields, &feature));
    if (feature.has_id) {
        gs["id"] = feature.id;
    }
    gs["type"] = feature.type;
    gs["tags"] = tags;
    gs["geometry"] = geometries;
    f->add(gs);
    LOG_DEBUG("feature: id=%u type=%s", (uint32_t) feature.id, geometry_type[feature.type]);
    return true;
}


pb_byte_t buffer[1024];

static bool decode_key(pb_istream_s *stream, const pb_field_iter_s *s, void**arg) {
    JsonArray *ka = reinterpret_cast<JsonArray *>(*arg);
    size_t len = stream->bytes_left;
    if (len > sizeof(buffer))
        return false;
    if (!pb_read(stream, buffer, stream->bytes_left))
        return false;
    string key(reinterpret_cast<const char *>(buffer), len);
    ka->add(key);
    return true;
}


static bool decode_value(pb_istream_s *stream, const pb_field_iter_s *s, void**arg) {
    JsonArray *va = reinterpret_cast<JsonArray *>(*arg);;
    Tile_Value v = Tile_Value_init_zero;
    CHECK(pb_decode(stream, Tile_Value_fields, &v));
    if (v.has_string_value) {
        va->add(v.string_value);
        return true;
    }
    if (v.has_float_value) {
        va->add(v.float_value);
        return true;
    }
    if (v.has_double_value) {
        va->add(v.double_value);
        return true;
    }
    if (v.has_int_value) {
        va->add(v.int_value);
        return true;
    }
    if (v.has_uint_value ) {
        va->add(v.uint_value);
        return true;
    }
    if (v.has_sint_value) {
        va->add(v.sint_value);
        return true;
    }
    if (v.has_bool_value) {
        va->add(v.bool_value);
        return true;
    }
    return false;
}

static bool decode_layers(pb_istream_s *stream, const pb_field_iter_s *s, void**arg) {
    Tile_Layer layer = Tile_Layer_init_zero;

    JsonArray *la = reinterpret_cast<JsonArray *>(*arg);
    JsonDocument f,l,k,v,t;
    JsonArray features = f.to<JsonArray>();
    JsonArray keys = k.to<JsonArray>();
    JsonArray values = v.to<JsonArray>();
    JsonArray tags = t.to<JsonArray>();

    layer.features.funcs.decode = &decode_feature;
    layer.features.arg = &features;

    layer.keys.funcs.decode = &decode_key;
    layer.keys.arg = &keys;

    layer.values.funcs.decode = &decode_value;
    layer.values.arg = &values;

    CHECK(pb_decode(stream, Tile_Layer_fields, &layer));
    LOG_DEBUG("Layer %s extent %u version: %u", layer.name, layer.extent, layer.version);

    l["name"] = layer.name;
    l["version"] = layer.version;
    if (layer.has_extent)
        l["extent"] = layer.extent;
    l["features"] = features;
    l["keys"] = keys;
    l["values"] = values;
    l["tags"] = tags;
    la->add(l);
    return true;
}


bool decode_mvt(char *blob, size_t blob_size) {
    pb_istream_t istream = pb_istream_from_buffer((const pb_byte_t *)blob,
                           blob_size);
    Tile decoded = Tile_init_zero;
    JsonDocument f;
    JsonArray layers = f.to<JsonArray>();

    decoded.layers.funcs.decode = &decode_layers;
    decoded.layers.arg = &layers;

    if (pb_decode(&istream, &Tile_msg, &decoded)) {
        size_t sz = 1024*1024;
        void *buf = malloc(sz);

        serializeJsonPretty(layers, buf, sz); //  IO);
        LOG_INFO("%s", (char *)buf);

        return true;
    } else {
        LOG_ERROR("%s fail", __FUNCTION__);
        return false;
    }
}

int main(int argc, char *argv[]) {

    set_loglevel(LOG_LEVEL_INFO);
    struct stat st;
    const char *file_name;
    if (argc != 2) {
        file_name = "data/test.mvt";
    } else {
        file_name = argv[1];
    }
    int fd = open (file_name, O_RDONLY);
    if (fd < 0) {
        perror(file_name);
        exit(1);
    }
    if (fstat(fd, &st)) {
        perror(file_name);
        exit(1);
    }
    char *map = static_cast<char *>(mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    return !decode_mvt(map, st.st_size);
}
