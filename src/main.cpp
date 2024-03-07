#include "FS.h"
#include <SdFat.h>
#include <M5Unified.h>
#include <SPI.h>
#include <Esp.h>

#include <SimpleSerialShell.h>
#include "logging.hpp"
#include "protomap.hpp"
#include "slippytiles.hpp"
#include "elevation.hpp"

double lat,lon,ref;
mapInfo_t *mi;
tile_t *tile;
extern SdFs sd;
extern FsFile file;
extern M5GFX display;

void setup_shell(void);
bool init_sd_card(void);
void draw_tile(tile_t *tile);
void setup_draw(void);
void loop_draw(void);

const char *boardType(void);

void setup(void) {
#ifdef ARDUINO_USB_CDC_ON_BOOT
    delay(3000);
#endif
#ifdef M5UNIFIED
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    M5.Display.init();
#else
    Serial.begin(115200);
#endif
    set_loglevel(LOG_LEVEL_INFO);
    LOG_INFO("board type: %s", boardType());

    init_sd_card();
    setup_draw();
    setup_shell();

}

int readMap(int argc, char** argv) {
    if (argc < 2) {
        return -1;
    }
    mi = addMap(argv[1]);
    if (mi == nullptr) {
        LOG_ERROR("addMap failed");
    } else {
        LOG_INFO("%s: zoom %d..%d resolution: %.2fm/pixel coverage %.2f/%.2f..%.2f/%.2f type %s",
                 mi->path.c_str(), mi->header.min_zoom, mi->header.max_zoom,
                 meters_per_pixel(mi),
                 min_lat(mi), min_lon(mi),
                 max_lat(mi), max_lon(mi),
                 tileType(mi->header.tile_type));
    }
    return 0;
};

int lsDir(int argc, char** argv) {
    sd.ls(LS_DATE|LS_SIZE|((argc == 1) ? LS_R : 0));
    return 0;
};

int metaData(int argc, char** argv) {
    if (mi) {
        parseMetadata(mi);
        serializeJsonPretty(mi->metadata, Serial);
        Serial.println();
        return 0;
    }
    return -1;
};

int mapInfo(int argc, char** argv) {
    dumpMaps();
    return 0;
};

int showCache(int argc, char** argv) {
    dumpCache();
    return 0;
};

int emptyCache(int argc, char** argv) {
    clearCache();
    return 0;
};


int tileByZoomLatLong(int argc, char** argv) {
    int zoom = -1;
    if (!mi) {
        LOG_INFO("no map");
        return -1;
    }
    if (argc == 4) {
        zoom = atoi(argv[1]);
        lat = atof(argv[2]);
        lon = atof(argv[3]);
    } else {
        zoom = mi->header.max_zoom;
        LOG_INFO("using max_zoom %u",zoom);
        lat = atof(argv[1]);
        lon = atof(argv[2]);
    }
    uint32_t offset_x, offset_y;
    tileStatus_t st = fetchTileByLatLong(*mi, &tile,  lat, lon, zoom, offset_x, offset_y);
    LOG_INFO("%s", tileStatus(st));
    LOG_DEBUG("map: %s ", mi->path.c_str());
    if (tile != nullptr) {
        LOG_DEBUG("tile xyz: %u %u %u offset %u %u type %s",
                  tile->zoom, tile->tile_x, tile->tile_y,  tile->offset_x, tile->offset_y, tileType(tile->tile_type));
        switch (tile->tile_type) {
            case  TILETYPE_MVT:
                break;
            case TILETYPE_PNG:
            case TILETYPE_JPEG:
            case TILETYPE_WEBP:
            case TILETYPE_AVIF:
                draw_tile(tile);
                break;
        }
    }
    return 0;
};

int tileByZXY(int argc, char** argv) {
    shell.print(__FUNCTION__);
    return 0;
};


int tileInfo(int argc, char** argv) {
    shell.print(__FUNCTION__);
    return 0;
};


int lookupElevation(int argc, char** argv) {
    if (argc != 3) {
        Serial.println("syntax: ele <lat> <lon>");
        return -1;
    }
    // tileInfo_t li = {};
    lat = atof(argv[1]);
    lon = atof(argv[2]);
    TIMESTAMP(ele);
    STARTTIME(ele);
    locationInfo_t li = {};

    if (!getElevation(lat, lon,  li)) {
        LOG_ERROR("no raster map found covering %f/%f", lat, lon);
        return -1;
    }
    if (li.status == LS_VALID) {
        LOG_INFO("elevation at %.4f,%.4f: %.2f meter", lat, lon, li.elevation);
        LOG_DEBUG("map: %s ", mi->path.c_str());
        LOG_DEBUG("tile xyz: %u %u %u offset %u %u",
                  li.tile->zoom, li.tile->tile_x, li.tile->tile_y,  li.tile->offset_x, li.tile->offset_y);
        return 0;
    } else {
        LOG_ERROR("getElevation: %s", locationStatus(li.status));
        return -1;
    }
};


int memoryUsage(int argc, char** argv) {
    LOG_INFO("free heap: %lu", ESP.getFreeHeap());
    LOG_INFO("used psram: %lu", ESP.getPsramSize() - ESP.getFreePsram());
    return 0;
};

int logLevel(int argc, char** argv) {
    if (argc == 1) {
        Serial.printf("log level: %d\n", get_loglevel());
    } else {
        set_loglevel((logLevel_t)atoi(argv[1]));
    }
    return 0;
};

int mountSD(int argc, char** argv) {
    init_sd_card();
    return 0;
};

int comment(int argc, char **argv) {
    auto lastArg = argc - 1;
    for ( int i = 0; i < argc; i++) {
        shell.print(argv[i]);
        if (i < lastArg)
            shell.print(F(" "));
    }
    shell.println();
    return 0;
}

void setup_shell(void) {
    shell.attach(Serial);
    shell.addCommand(F("? : print help"), shell.printHelp);
    shell.addCommand(F("map <filename> : load a map"), readMap);
    shell.addCommand(F("ls  [dir] : list directory "), lsDir);
    shell.addCommand(F("lm  : list maps loaded "), mapInfo);
    shell.addCommand(F("meta  : dump map metadata"), metaData);
    shell.addCommand(F("tile [<zoom>] <lat> <lon>  : retrieve tile by coordinate (zoom is optional, defaults to maxzoom)"), tileByZoomLatLong);
    shell.addCommand(F("zxy <z> <x> <y>  : retrieve tile by z x y"), tileByZXY);
    shell.addCommand(F("ti  : show tile information"), tileInfo);
    shell.addCommand(F("ele   <lat> <lon> : display elevation at lat/lon"), lookupElevation);
    shell.addCommand(F("mem : display memory usage"), memoryUsage);
    shell.addCommand(F("log <level> : set log level, 0=None,1=Error,2=Warn,3=Info,4=Debug,5=Verbose"), logLevel);
    shell.addCommand(F("sd : mount SD card"), mountSD);
    shell.addCommand(F("lc : list tile cache"), showCache);
    // shell.addCommand(F("cc : clear tile cache"), emptyCache);
    shell.addCommand(F("# text : comment line"), comment);

    shell.printHelp(0, NULL);
    shell.print("> ");
}

void loop(void) {
    M5.update();
    shell.executeIfInput();
    loop_draw();
    delay(1);
}