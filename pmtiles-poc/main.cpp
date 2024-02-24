#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <string_view>

#include "zlib.h"

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

#include "pmtiles.hpp"
#include "buffer_ref.h"

#define xstr(a) str(a)
#define str(a) #a

typedef mqtt::buffer_ref<char> buffer_ref;
typedef mqtt::buffer_ref<uint8_t> blob_ref;

#define MIN_ALLOC_SIZE 8192
#define MOD_GZIP_ZLIB_WINDOWSIZE 15

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

using namespace pmtiles;
using namespace std;

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

const char * file_name = xstr(PMTILES_PATH);

static inline double rgb2alt( uint8_t r, uint8_t g, uint8_t b) {
    return  -10000 + ((r * 256 * 256 + g * 256 + b) * 0.1);
}

int32_t decodeTile(const buffer_ref b) {
    size_t blob_size = b.size();
    const uint8_t *blob = reinterpret_cast<const uint8_t *>(b.data());
    int width, height, rc;
    VP8StatusCode sc;
    WebPDecoderConfig config;
    size_t bufsize;
    uint8_t *buffer;

    WebPInitDecoderConfig(&config);
    rc = WebPGetInfo(blob, blob_size, &width, &height);
    if (!rc) {
        fprintf(stderr, "WebPGetInfo failed rc=%d\n", rc);
        return -1;
    }
    sc = WebPGetFeatures(blob, blob_size, &config.input);
    if (sc != VP8_STATUS_OK) {
        fprintf(stderr, "WebPGetFeatures failed sc=%d\n", sc);
        return -2;
    }
    if (config.input.format != 2) {
        fprintf(stderr, "lossy WEBP compression");
        return -3;
    }
    bufsize = width * height * 3;
    buffer = reinterpret_cast<uint8_t *>(malloc(bufsize));

    if (WebPDecodeRGBInto(blob, blob_size,
                          buffer, bufsize, width * 3) == nullptr) {
        fprintf(stderr, "WebPDecode failed");
        return -4;
    } else {
        // printf("size %u fed %zu\n\n", blob.size(), fed);
        int x = 27;
        int y = 6;
        size_t offset = (x + config.input.width * y) * 3;

        uint8_t r  = buffer[offset];
        uint8_t g  = buffer[offset+1];
        uint8_t b  = buffer[offset+2];
        double elevation = rgb2alt(r,g,b);
        printf("elevation=%.1f\n\n", elevation);
    }
    WebPFreeDecBuffer(&config.output);
}

bool isNull(const entryv3 &e) {
    return ((e.tile_id == 0) &&  (e.offset == 0) &&
            (e.length == 0) && (e.run_length == 0));
}

static size_t decomp_size;
static char *decomp_buffer;

#define GUESS 2
pmErrno_t decompress_gzip(const string_view str, buffer_ref &out) {

    if (decomp_size < str.size()) {
        decomp_size = alloc_size(str.size() * GUESS);
        decomp_buffer = reinterpret_cast<char *>(realloc(decomp_buffer, decomp_size));
        if (decomp_buffer == nullptr) {
            fprintf(stderr, "realloc %zu -> %zu failed:  %s\n", str.size(), decomp_size, strerror(errno));
            pmerrno = PM_DECOMP_BUF_ALLOC_FAILED;
            if (decomp_buffer)
                free(decomp_buffer);
            decomp_size = 0;
            return PM_DECOMP_BUF_ALLOC_FAILED;
        }
    }

    z_stream zs = {};
    int32_t rc = inflateInit2(&zs, MOD_GZIP_ZLIB_WINDOWSIZE + 16);

    if (rc != Z_OK) {
        fprintf(stderr, "inflateInit failed");
        return PM_DEFLATE_INIT_FAILED;
    }

    zs.next_in = (Bytef *)str.data();
    zs.avail_in = str.size();

    do {
        zs.next_out = reinterpret_cast<Bytef*>(decomp_buffer + zs.total_out);
        zs.avail_out = decomp_size - zs.total_out;

        rc = inflate(&zs, 0);

        if (decomp_size <= zs.total_out) {
            decomp_size = alloc_size(zs.total_out*2);
            decomp_buffer = reinterpret_cast<char *>(realloc(decomp_buffer, decomp_size));
        }

    } while (rc == Z_OK);

    inflateEnd(&zs);

    if (rc != Z_STREAM_END) {
        fprintf(stderr, "zlib decompression failed : %d\n", rc);
        return PM_ZLIB_DECOMP_FAILED;
    }
    out = buffer_ref(decomp_buffer, zs.total_out);
    return PM_OK;
}

static size_t io_size;
static char *io_buffer;

static pmErrno_t get_bytes(FILE *fp,
                           off_t start,
                           size_t length,
                           buffer_ref &result) {

    if (io_size < length) {
        io_size = alloc_size(length);
        io_buffer = reinterpret_cast<char *>(realloc(io_buffer, io_size));
        if (io_buffer == nullptr) {
            fprintf(stderr, "realloc %zu failed:  %s\n", length, strerror(errno));
            pmerrno = PM_IOBUF_ALLOC_FAILED;
            io_size = 0;
            return PM_IOBUF_ALLOC_FAILED;
        }
    }

    off_t ofst = fseek(fp, start, SEEK_SET);
    if (ofst != 0 ) {
        fprintf(stderr, "fseek to %lld failed:  %s\n", start, strerror(errno));
        return PM_SEEK_FAILED;;
    }
    size_t got = fread(io_buffer, 1, length, fp);
    if (got != length) {
        fprintf(stderr, "read failed: got %zu of %zu, %s\n", got, length, strerror(errno));
        return PM_READ_FAILED;
    }
    result = buffer_ref(io_buffer, length);
    return PM_OK;
}

int main(int argc, char *argv[]) {
    int32_t rc;

    printf("C++ version: %ld\n", __cplusplus);
    struct stat st;
    if (argc > 1)
        file_name = argv[1];
    int fd = open (file_name, O_RDONLY);
    if (fd < 0) {
        perror(file_name);
        exit(1);
    }
    if (fstat(fd, &st)) {
        perror(file_name);
        exit(1);
    }

    FILE *fp = fopen(file_name, "rb");
    if (fp == nullptr) {
        perror(file_name);
        exit(1);
    }
    printf("open '%s' size %lld\n", file_name, st.st_size);

    // test example - knowns:
    //   lat lon
    //   actual altitude
    //   z,x,y
    //   x,y offsets in resulting tile
    //
    // lat = 47.12925176802318;
    // lon = 15.209778656353123;
    // refalt = 865.799987792969 meters
    // tile_x=4442.0 tile_y=2877.0 offset_x=27.38257980090566 offset_y=5.985081114224158 zoom_level=13
    // -> 13, 4442, 2877 offset 27,6

    // Executing .pio/build/pmtiles/program
    // size 558458853 buf 0x10b641000
    // tid 79295500
    // init w 256 h 256
    // size 79198 fed 79198
    // elevation=863.3
    // decomp.reserve(1024);
    int8_t pmerr;

    uint64_t tile_id = zxy_to_tileid(13, 4442, 2877, pmerr);
    if (pmerr) {
        fprintf(stderr, "zxy_to_tileid failed : %d\n", pmerr);
        exit(1);
    }
    printf("tid %lld\n", tile_id);

    buffer_ref io;
    if ((rc = get_bytes(fp, 0, 127, io)) != PM_OK) {
        perror("get_bytes");
        exit(1);
    }
    headerv3 header = deserialize_header(string_view(io.to_string()), pmerr);
    if (pmerr) {
        fprintf(stderr, "deserialize_header failed : %d\n", pmerr);
        exit(1);
    }
    uint64_t dir_offset  = header.root_dir_offset;
    uint64_t dir_length = header.root_dir_bytes;
    buffer_ref decomp;
    for (int i = 0; i < 4; i++) {
        if ((rc = get_bytes(fp, dir_offset, dir_length, io)) != PM_OK) {
            perror("get_bytes");
            break;
        }
        std::vector<entryv3> directory;
        if (header.internal_compression == COMPRESSION_GZIP) {
            rc = decompress_gzip(string_view(io.to_string()), decomp);
            if (rc != PM_OK) {
                fprintf(stderr, "decompress_gzip failed : %d\n", rc);
                exit(1);
            }
            directory = deserialize_directory(decomp.to_string(), pmerr);
        } else {
            directory = deserialize_directory(io.to_string(), pmerr);
        }
        if (pmerr) {
            fprintf(stderr, "deserialize_directory failed : %d\n", pmerr);
            exit(1);
        }
        entryv3 result = find_tile(directory,  tile_id);

        if (!isNull(result)) {
            if (result.run_length == 0) {
                dir_offset = header.leaf_dirs_offset + result.offset;
                dir_length = result.length;
            } else {
                if ((rc = get_bytes(fp, header.tile_data_offset + result.offset, result.length, io)) != PM_OK) {
                    perror("get_bytes");
                    break;
                }
                decodeTile(io);
                break;
            }
        }
    }
    return 0;
}
