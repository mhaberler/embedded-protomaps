# embedded-protomaps: elevation lookup

given latitude and longitude, and a Digital Elevation Model in GeoTIFF format, this code retrieves the elevation on a resource-constrained platform - i.e. assuming GDAL is not available

This is the embedded equivalent to
````
$ gdallocationinfo -wgs84  DTM_Austria_10m_v2_by_Sonny.tif 15.20977 47.12925
Report:
  Location: (43264P,21366L)
  Band 1:
    Value: 865.799987792969   <--- elevation in meters
````

## use case
- given coordinats from a GPS, this code should give much better vertical accuracy than the GPS elevation
- for flying, this is a basis for determining height-over-ground using a GPS
- the tile retrieval code could be used for map display on embedded platforms.

## Design
- choose a DEM of the desired resolution or higher. For this example, I took the [Austria 10m DEM published by Sonny](https://sonny.4lima.de/). 
- choose a [zoom level matching the desired resolution](https://wiki.openstreetmap.org/wiki/Zoom_levels) (in my case: zoom 13 results in about 14m/pixel covering an area of 3.6km squared per tile)
- convert the DEM into [Terrain-RGB](https://github.com/syncpoint/terrain-rgb/blob/master/README.md) format stored in an [Protomaps](https://protomaps.com/) file, breaking up a large GeoTIFF into small tiles at the chosen zoom level
- compress the tiles with [WEBP-lossles](https://developers.google.com/speed/webp) compressor
- decoded tiles are LRU-cached. A typical 256x256 tile requires about 200kB.

DEMs are stored as single files in [pmtiles format](https://guide.cloudnativegeo.org/pmtiles/intro.html) on a SD card. See below for file samples.


## Use a ready-made sample:

A DEM file built with the steps described in the next section can be found [here](https://static.mah.priv.at/cors/DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles). (367MB)


## or roll your own:

These steps outline how to convert a GeoTIFF DEM into a Terrain-RGB pmtiles archive.

First, install these prerequisites:

`pip install gdal rio-rgbify`

Install the [PMTiles CLI](https://docs.protomaps.com/guide/getting-started).
Make sure the [sqlite3 command line tool](https://www.sqlitetutorial.net/download-install-sqlite/) is installed.


Here is a [tutorial outlining the approach](https://github.com/syncpoint/terrain-rgb/blob/master/README.md) - I used these steps:

`````
# reproject into Web Mercator
gdalwarp \
    -t_srs EPSG:3857  \
    -dstnodata None  \
    -novshiftgrid \
    -co TILED=YES  \
    -co COMPRESS=DEFLATE  \
    -co BIGTIFF=IF_NEEDED \
    -r lanczos \
    DTM_Austria_10m_v2_by_Sonny.tif \
    DTM_Austria_10m_v2_by_Sonny_3857.tif

# convert the elevation (float) into RGB bands (8bit each)
# adapt --workers for your platform:
rio rgbify   \
    --base-val -10000   \
    --interval 0.1  \
    --workers 8 \
    DTM_Austria_10m_v2_by_Sonny_3857.tif  \
    DTM_Austria_10m_v2_by_Sonny_3857_RGB.tif

# break up into small tiles
gdal2tiles.py --xyz --zoom=13-13 --tiledriver=WEBP --webp-lossless --processes=8  DTM_Austria_10m_v2_by_Sonny_3857_RGB.tif ./tiles-webp

# combine the tile tree into an MBtiles file
mb-util --silent --image_format=webp --scheme=xyz ./tiles-webp/ DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.mbtiles

# fix the metadata so pmtiles recognizes the webp format:
sqlite3 DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.mbtiles <<EOF
DELETE FROM metadata;
INSERT INTO metadata 
    ( name, value) 
    VALUES 
        ('format', 'webp'),
        ('minzoom', '13'),
        ('maxzoom', '13'),
        ('name', 'Austria 10m DEM'),
        ('description', 'https://sonny.4lima.de/'),
        ('bounds','9.31119385878015393,46.2378907773286869,17.250875865616095,49.0517914376430895');
EOF

# convert the MBtiles file in Protomaps format
pmtiles convert DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.mbtiles DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles

# sanity check: test-extract a known-to-exist tile with some visible content, then try in an image viewer:
pmtiles tile DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles 13 4442 2877 >tile.webp
`````



## Test setup

Copy `DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles` file to a fast SD card.

This code was tested on a M5Stack CoreS3, using webp format pmtiles. Default cache size is 5 tiles, using 1M PSRAM.

A Python PoC implementation can be found [here](python/pmgetaltitude.py) .

## Building:

This is a PlatformIO project:

`git clone --recursive https://github.com/mhaberler/embedded-mbtiles.git`

Targets: `coreS3-pmtiles-example` . 

The `pmtiles-poc-macos` is a proof-of-concept native C++ program (MacOS).

## Running the example

Build and upload, and insert the SD card.
Console log:
````
Reconnecting to /dev/cu.usbmodem2131401          Connected!
C++ version: 202100
free heap: 382668
used psram: 2312
SD card detected, mounting..
/sd/DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles: zoom 13..13 resolution: 12.87m/pixel coverage 46.24/9.31..49.05/17.25 type WEBP
blob retrieve 385356 us
WEBP decode 128821 us
8113 Stiwoll Kehrer:  0 1 867.10 865.80 - 514768 uS cold
8113 Stiwoll Kehrer:  0 1 867.10 865.80 - 244 uS cached
blob retrieve 276558 us
WEBP decode 122355 us
1180 Utopiaweg 1:  0 1 332.80 333.00
blob retrieve 303468 us
WEBP decode 122252 us
1190 Höhenstraße:  0 1 402.80 403.60
blob retrieve 223234 us
WEBP decode 113274 us
1010 Stephansplatz:   0 1 171.30 171.40
blob retrieve 206312 us
WEBP decode 104652 us
1220 Industriestraße 81:  0 1 158.60 158.60

cached tiles:
dem=0 13/4470/2839
dem=0 13/4468/2840
dem=0 13/4466/2838
dem=0 13/4466/2839
dem=0 13/4442/2877
DEMS available:
dem 0: /sd/DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles coverage 46.24/9.31..49.05/17.25 tile_decode_err=0 protomap_err=0 hits=1 misses=5 tilesize=256 type WEBP
free heap: 381036
used psram: 1111008
````

## Other platforms

current dependencies are:
- stdio fseek/fread for file access - see the `get_bytes` method in src/protomap.cpp
- PSRAM heap allocation (currently ESP-IDF specifc) - see `heap_caps_*` calls in src/protomap.cpp and src/compress.cpp

This should run with minor mods on any 32bit embedded platform with sufficient RAM to hold a tile. The
code supports webp and png decompression. However, there is no good reason to actually use png format - much larger files and slower decompression.


## Performance

Reading a tile from SD: 
- 470 ms for the first tile using webp lossles
- of which 128ms are for webp decompression
- looking up elevation on a cached tile: less than 0.5ms.

SD card used was a Transcend Ultimate 633x, 32GB, FAT32 filesystem. A faster SD card probably helps.

## Usage example

see `src/main.cpp`.

## Status
works fine, but very C-ish code.


## NODATA values

These are reported as altitude 0.0m.


## parts list
- a slightly modified version of https://github.com/protomaps/PMTiles/blob/main/cpp/pmtiles.hpp  to not throw exceptions
- decoding PNG tiles: [pngle](https://github.com/kikuchan/pngle)
- decoding webp tiles: [libwebp](https://github.com/webmproject/libwebp)
- LRU cache for decoded PNG tiles: a modified version of [cpp-lru-cache](https://github.com/lamerman/cpp-lru-cache)

