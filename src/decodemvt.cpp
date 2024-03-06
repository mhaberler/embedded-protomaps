#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>
#include <ArduinoJson.h>

#include <pb_encode.h>
#include <pb_decode.h>
#include <pb_common.h>

#include "vector_tile.pb.h"
#include "protomap.hpp"
#include "logging.hpp"


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
static int64_t now;

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
    uint32_t m_max_count = m_end/2;
    int32_t cursorX = 0;
    int32_t cursorY = 0;
    while (m_it < m_end) {
        uint32_t command = geom[m_it++];
        uint32_t command_id = command & 0x7;
        uint32_t m_count = command >> 3;
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
    vector_tile_Tile_Feature feature = vector_tile_Tile_Feature_init_zero;

    feature.geometry.funcs.decode = decode_geometry;
    feature.geometry.arg = &geometries;

    feature.tags.funcs.decode = &decode_tag;
    feature.tags.arg = &tags;

    CHECK(pb_decode(stream, vector_tile_Tile_Feature_fields, &feature));
    if (feature.has_id) {
        gs["id"] = feature.id;
    }
    gs["type"] = feature.type;
    gs["tags"] = tags;
    gs["geometry"] = geometries;
    f->add(gs);
    return true;
}

static bool decode_key(pb_istream_s *stream, const pb_field_iter_s *s, void**arg) {
    JsonArray *ka = reinterpret_cast<JsonArray *>(*arg);
    string_view key((const char*)stream->state, stream->bytes_left);
    if (!pb_read(stream, NULL, stream->bytes_left))
        return false;
    ka->add(key);
    return true;
}


static bool decode_value(pb_istream_s *stream, const pb_field_iter_s *s, void**arg) {
    JsonArray *va = reinterpret_cast<JsonArray *>(*arg);;
    vector_tile_Tile_Value v = vector_tile_Tile_Value_init_zero;
    CHECK(pb_decode(stream, vector_tile_Tile_Value_fields, &v));
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
    vector_tile_Tile_Layer layer = vector_tile_Tile_Layer_init_zero;

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

    CHECK(pb_decode(stream, vector_tile_Tile_Layer_fields, &layer));
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

tileStatus_t decode_mvt(const uint8_t *blob, size_t blob_size, JsonDocument &doc) {
    TIMESTAMP(__FUNCTION__);
    STARTTIME(__FUNCTION__);
    pb_istream_t istream = pb_istream_from_buffer((const pb_byte_t *)blob,
                           blob_size);
    vector_tile_Tile decoded = vector_tile_Tile_init_zero;
    doc.clear();
    JsonArray layers = doc.to<JsonArray>();

    decoded.layers.funcs.decode = &decode_layers;
    decoded.layers.arg = &layers;

    // see nanopb/tests/extensions/decode_extensions.c in case extension are required
    // pb_extension_t Tile_ext;
    // decoded.extensions  = &Tile_ext;

    if (pb_decode(&istream, &vector_tile_Tile_msg, &decoded)) {
        PRINT_LAPTIME("MVT decode %lu us", __FUNCTION__);

        LOG_DEBUG("%s success size", __FUNCTION__);
        serializeJson(layers, Serial);
        // serializeJsonPretty(layers, Serial);
        Serial.println();
        return TS_MVTDECODE_OK;
    } else {
        LOG_ERROR("%s fail", __FUNCTION__);
        return TS_MVTDECODE_FAILED;
    }
}

