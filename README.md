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
- compress the tiles with [WEBP-lossles](https://developers.google.com/speed/webp) compressor (PNG is also supported but not recommended - larger and slower)
- decoded tiles are LRU-cached. A typical 256x256 tile requires about 200kB.

DEMs are stored as single files in [pmtiles format](https://guide.cloudnativegeo.org/pmtiles/intro.html) on a SD card. See below for file samples.

### Prerequisites for building a DEM

`pip install gdal rio-rgbify`

Install the [PMTiles CLI](https://docs.protomaps.com/guide/getting-started).

## Converting the GeoTIFF DEM into a Terrain-RGB MBTiles archive

Here is a [tutorial outlining the approach](https://github.com/syncpoint/terrain-rgb/blob/master/README.md) - I used these steps:

`````
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

# adapt --workers for your platform:
rio rgbify   \
    --base-val -10000   \
    --interval 0.1  \
    --workers 8 \
    DTM_Austria_10m_v2_by_Sonny_3857.tif  \
    DTM_Austria_10m_v2_by_Sonny_3857_RGB.tif

gdal2tiles.py --zoom=13-13 --tiledriver=WEBP --webp-lossless --processes=8  DTM_Austria_10m_v2_by_Sonny_3857_RGB.tif ./tiles
mb-util --silent --image_format=webp --scheme=xyz ./tiles/ DTM_Austria_10m_v2_by_Sonny_3857_RGB_13.mbtiles
pmtiles convert DTM_Austria_10m_v2_by_Sonny_3857_RGB_13.mbtiles DTM_Austria_10m_v2_by_Sonny_3857_RGB_13.pmtiles
`````
Copy the `.pmtiles` file to a fast SD card and set the name in `platformio.ini` accordingly.


## Platform

This code was tested on a M5Stack CoreS3 but should run on any ESP32 platform with an SD card reader and sufficient PSRAM. 

Default cache size is 5 tiles, using 1M PSRAM.

A Python PoC implementation is [here](python/pmgetaltitude.py)

## Building:

This is a PlatformIO project,

`git clone --recursive https://github.com/mhaberler/embedded-mbtiles.git`

Targets: `coreS3-pmtiles-example` . 

The `pmtiles-poc-macos` is just a proof-of-concept native C++ program (MacOS).

## Other platforms

current dependencies are:
- stdio fseek/fread for file access - see the `get_bytes` method in src/protomap.cpp
- PSRAM heap allocation (currently ESP-IDF specifc) - see `heap_caps_*` calls in src/protomap.cpp and src/compress.cpp

This should run with minor mods on any platform with sufficient RAM to hold a tile.


## Performance

Reading a tile from SD: 470 ms for the first tile using webp lossles - of which 128ms are for webp decompression
Looking up elevation on a cached tile is fast - less than 0.5ms.

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

