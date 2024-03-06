Steps to build and exercise the Epaper demo

1. prepare an SD  card. I formatted a 400GB SanDisk Extreme as exFat filesystem with MacOS Disk Utility.
2. copy some pmtiles files to the card. For my elevation demo, use https://static.mah.priv.at/cors/DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles .
3. in VSCode, install the Platformio extension.
4. `git clone https://github.com/mhaberler/embedded-protomaps --branch e5paper`
5. `git submodule update --init`
6. open the repo in VSCode
7. what now happens will take a while as platformio downloads toolchains, libraries etc
8. with the plug icon at the bottom, select the epaper serial port. Mine looks like so: cu.wchusbserial57130467711
9. build by clicking the checkmark icon
10. upload by clicking the right arrow icon
11. connect to the console by clicking the plug icon
12. hit the e5paper reset button
13. console output should look like so:

`````
*  Executing task: platformio device monitor --environment epaper-pmtiles-example --port /dev/cu.wchusbserial57130467711 

--- Terminal on /dev/cu.wchusbserial57130467711 | 115200 8-N-1
--- Available filters and text transformations: colorize, debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
board type: Paper
SD card mounted successfully
card size: 394 GB
Card type: SDHC/SDXC
FS type: exFat
Manufacturer ID: 0x3
OEM ID: 0x53 0x44
Product: 'SN400'
Revision: 8.0
Serial number: 332833043
Manufacturing date: 7/2020
Commands available are:
  ? : print help
  display  : show tile information
  ele   <lat> <lon> : display elevation at lat/lon
  help
  lc : list tile cache
  lm  : list maps loaded 
  log <level> : set log level, 0=None,1=Error,2=Warn,3=Info,4=Debug,5=Verbose
  ls  [dir] : list directory 
  map <filename> : load a map
  mem : display memory usage
  meta  : dump map metadata
  sd : mount SD card
  tile [<zoom>] <lat> <lon>  : retrieve tile by coordinate (zoom is optional, defaults to maxzoom)
  zxy <z> <x> <y>  : retrieve tile by z x y

# list files on the SD card:

> ls
2024-02-24 17:57    384676568 DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles
2024-02-27 17:03    496658390 slovenia.pmtiles
2023-05-24 01:09 110147520430 protomaps-basemap-opensource-20230408.pmtiles
2023-03-21 14:44   9818653875 africa-latest.pmtiles
2023-08-29 00:17  10055511763 basemapat_Ortho16.pmtiles

# open a map on SD - this being a raster DEM

> map  DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles
DTM_Austria_10m_v2_by_Sonny_3857_RGB_13_webp.pmtiles: zoom 13..13 resolution: 12.87m/pixel coverage 46.24/9.31..49.05/17.25 type WEBP

# lookup elevation at my home Location

> ele 47 15
blob retrieve 95081 us
blob decompress 171618 us
elevation at 47.0000,15.0000: 895.30 meter

# open an OSM vector pmtiles map (110GB)

> map protomaps-basemap-opensource-20230408.pmtiles
protomaps-basemap-opensource-20230408.pmtiles: zoom 0..15 resolution: 4.78m/pixel coverage -90.00/-180.00..90.00/180.00 type MVT

# lookup a location:

> tile 47 15
using max_zoom 15
blob retrieve 7612509 us
tile decompress 2343 us
MVT decode 35095 us
[{"name":"roads","version":2,"extent":4096,"features":[[{"op":"move_to","coords":[2106,323]},{"op":"line_to","coords":[115,158]}],[{"op":"move_to","coords":[1062,-64]},{"op":"line_to","coords":[733,226,205,90,106,71]},{"op":"move_to","coords":[115,158]},{"op":"line_to","coords":[68,163,73,127,120,124,157,97,160,57,171,9,340,-55,344,-73,314,-40,192,-49]}],[{"op":"move_to","coords":[2238,521]},{"op":"line_to","coords":[-76,-13,-134,16,-190,-8,-344,-42,-154,-44,-109,-26,-110,-20,-127,-8,-302,42,-139,12,-177,-4,-102,2,-121,42,-183,54,-34,13]},{"op":"move_to","coords":[2099,-262]},{"op":"line_to","coords":[54,-69,52,-60,65,-43,35,-29]},{"op":"move_to","coords":[1448,2664]},{"op":"line_to","coords":[26,15,38,-1,115,-38,58,-26,29,-27,16,-30,31,-110]},{"op":"move_to","coords":[144,-263]},{"op":"line_to","coords":[14,-62]}],[{"op":"move_to","coords":[3688,802]},{"op":"line_to","coords":[-48,-86]}],[{"op":"move_to","coords":[3640,716]},{"op":"line_to","coords":[-172,-312,116,-192,-8,-55,-76,27,-152,176,-300,176,-84,24,-52,-36,28,-120,116,-159,180,-136,205,-173]},{"op":"move_to","coords":[719,875]},{"op":"line_to","coords":[-389,20,-71,-7,-12,-22]}],[{"op":"move_to","coords":[1146,3260]},{"op":"line_to","coords":[204,-8,174,-72,128,-10,336,-108,138,-120,236,-230,128,-38,166,-12,106,-64,108,-72,170,-40,116,-124,76,-16,50,-74,-8,-198,-38,-114,-102,-62,-234,-48,-178,-10,-136,-90,-48,-56,-166,-104,-88,-110,-114,-30,-112,-92,-112,-34,-138,16,-204,-34,-172,-70,-168,-108,-66,-2,-114,90,-82,154,-76,104,-116,44,-220,-2,-86,-36,-102,-102,-74,-10,-80,20,-168,116,-112,64,-32,7]},{"op":"move_to","coords":[0,291]},{"op":"line_to","coords":[152,30,106,32,82,32,64,40,78,70,48,60,96,156,80,148,36,98,38,116,38,78,126,196,252,308,14,30,4,26,-6,18,-52,92,-76,152,-30,76,-12,40,-6,50,4,28,24,46,25,26]},{"op":"move_to","coords":[-531,346]},{"op":"line_to","coords":[98,-78,29,-18,34,-8,34,0,110,22,83,32]},{"op":"move_to","coords":[-51,-3712]},{"op":"line_to","coords":[87,46,474,184,388,118,138,-6,88,22,126,126,340,284,214,78,120,19,304,-23,154,-26,344,-120,164,26,146,-2,70,-20,172,122,4,8]},{"op":"move_to","coords":[0,374]},{"op":"line_to","coords":[-116,-60,-222,-52,-180,-70,-222,-6,-164,38,-254,98,-188,4,-52,24,-30,64,-50,22,-144,24]},{"op":"move_to","coords":[1446,-590]},{"op":"line_to","coords":[82,-24,94,0]}],[{"op":"move_to","coords":[859,4160]},{"op":"line_to","coords":[74,-186,20,-41,45,-83,45,-66,54,-62,61,-55,28,-21,53,-27,68,-18,38,-6,38,-2,77,6,640,145,61,16,77,25,157,81,57,23,27,8,67,9,35,-3,55,-14,30,-11,247,-112,85,-48,37,-25,129,-100,67,-62,19,-25,15,-23,29,-64,87,-255,15,-32,58,-86,393,-515,101,-127,39,-46,26,-25,52,-39,24,-13,71,-29]}],[{"op":"move_to","coords":[523,429]},{"op":"line_to","coords":[-21,-20,-95,-49,-108,-64,-212,-62,-151,-22]}]],"keys":["ref","name","bridge","pmap:kind","pmap:level","layer"],"values":["L343","Hirscheggerstraße","yes","medium_road",1,"1",0,"minor_road","Pöschllenzweg","other","Packer Straße","B70","major_road","Koriweg"],"tags":[]},{"name":"natural","version":2,"extent":4096,"features":[[{"op":"move_to","coords":[2313,38]},{"op":"line_to","coords":[-64,117,-54,4,-17,46,-51,87,-121,9,-84,-23,-174,-71,-35,-99,-25,-57,26,-111,6,-4,404,0,113,22]},{"op":"close_path"}],[{"op":"move_to","coords":[4160,1838]},{"op":"line_to","coords":[0,1403,-87,-265,-58,0,-58,327,-286,17,-27,-116,-76,-89,-157,-134,228,-322,201,-349,-62,-223,125,-103]},{"op":"close_path"}],[{"op":"move_to","coords":[2602,2965]},{"op":"line_to","coords":[112,-27,241,389,76,250,-156,304,25,279,-1195,0,-139,-91,-54,-14,-98,-8,-103,-362,134,-121,407,80,62,143,233,49,-18,-129,-72,-45,-22,-299,344,-259]},{"op":"close_path"}],[{"op":"move_to","coords":[4160,672]},{"op":"line_to","coords":[0,162,-170,20,16,-68,94,-88]},{"op":"close_path"}],[{"op":"move_to","coords":[-64,-64]},{"op":"line_to","coords":[1784,0,-6,4,-26,111,25,57,35,99,174,71,84,23,121,-9,51,-87,17,-46,54,-4,64,-117,-76,-80,-113,-22,2036,0,0,124,-148,37,-216,7,-112,81,-172,243,-588,228,56,196,644,-220,536,-216,0,256,-60,26,-94,88,-16,68,170,-20,0,1004,-257,146,-125,103,62,223,-201,349,-228,322,157,134,76,89,27,116,286,-17,58,-327,58,0,87,265,0,919,-1260,0,-25,-279,156,-304,-76,-250,-241,-389,-112,27,-223,139,-344,259,22,299,72,45,18,129,-233,-49,-62,-143,-407,-80,-134,121,103,362,98,8,54,14,139,91,-1769,0]},{"op":"close_path"},{"op":"move_to","coords":[2354,-3455]},{"op":"line_to","coords":[-62,-93,-46,38,37,161,83,53,80,12,39,-6]},{"op":"close_path"}]],"keys":["landuse","pmap:area"],"values":["meadow",228125,1110018,2101840,30940,"forest",17842176],"tags":[]},{"name":"landuse","version":2,"extent":4096,"features":[[{"op":"move_to","coords":[2924,656]},{"op":"line_to","coords":[588,-228,172,-243,112,-81,216,-7,148,-37,0,356,-536,216,-644,220]},{"op":"close_path"}],[{"op":"move_to","coords":[2182,650]},{"op":"line_to","coords":[46,-38,62,93,131,165,-39,6,-80,-12,-83,-53]},{"op":"close_path"}]],"keys":["landuse","name","pmap:area","pmap:kind"],"values":["industrial","Holzlagerplatz",978912,"grass",63096,"other"],"tags":[]},{"name":"buildings","version":2,"extent":4096,"features":[[{"op":"move_to","coords":[2286,818]},{"op":"line_to","coords":[-12,-22,12,-6,11,21]},{"op":"close_path"}],[{"op":"move_to","coords":[2237,751]},{"op":"line_to","coords":[-13,-45,36,-11,15,45]},{"op":"close_path"}],[{"op":"move_to","coords":[2001,106]},{"op":"line_to","coords":[-15,-44,28,-10,6,16,35,-12,9,29,-17,6,2,6,-14,4,-2,-6]},{"op":"close_path"}],[{"op":"move_to","coords":[2162,103]},{"op":"line_to","coords":[-12,-23,52,-28,2,4,5,-3,8,16,-5,3,2,4]},{"op":"close_path"}],[{"op":"move_to","coords":[3938,2663]},{"op":"line_to","coords":[-60,-14,22,-96,60,14]},{"op":"close_path"}],[{"op":"move_to","coords":[3874,2823]},{"op":"line_to","coords":[-43,-42,56,-58,43,41]},{"op":"close_path"}],[{"op":"move_to","coords":[4020,2645]},{"op":"line_to","coords":[-37,-10,16,-57,18,5,15,-54,78,21,-29,106,-60,-16]},{"op":"close_path"}],[{"op":"move_to","coords":[4061,2519]},{"op":"line_to","coords":[-53,-44,43,-51,-34,-29,21,-25,34,29,64,-77,24,20,0,124,-11,-9,-8,10,-21,-18]},{"op":"close_path"}],[{"op":"move_to","coords":[3740,489]},{"op":"line_to","coords":[-27,-60,112,-52,27,61]},{"op":"close_path"}]],"keys":[],"values":[],"tags":[]},{"name":"physical_line","version":2,"extent":4096,"features":[[{"op":"move_to","coords":[3905,4160]},{"op":"line_to","coords":[38,-135,201,-193,16,-14]}],[{"op":"move_to","coords":[-64,-14]},{"op":"line_to","coords":[36,-24,43,-26]},{"op":"move_to","coords":[571,0]},{"op":"line_to","coords":[129,83,150,44,121,-20,138,0,147,115,171,89,181,39,208,107,259,52,142,-45,72,-70,45,-183,75,-115,77,4,91,0,92,46,42,105,-68,163,-114,122,-65,123,0,153,90,74,119,55,119,59,131,10,120,-28,109,-1,105,-43,100,-31,456,-144,83,-75,149,-80,100,-6]}],[{"op":"move_to","coords":[2668,923]},{"op":"line_to","coords":[42,-98]}],[{"op":"move_to","coords":[2738,-64]},{"op":"line_to","coords":[-3,23,-47,93,-4,30]}]],"keys":["name","pmap:kind","waterway"],"values":["Packer Bach","waterway","stream","Teigitsch","river","weir","Enzibach"],"tags":[]},{"name":"boundaries","version":2,"extent":4096,"features":[[{"op":"move_to","coords":[567,-64]},{"op":"line_to","coords":[112,78,141,39,227,-28,262,193,369,73,484,167,70,-37,115,-248,67,-54,200,5,54,69,-77,204,-114,197,-10,70,9,99,125,113,88,40,136,55,142,-4,150,-38,187,-30,227,-69,629,-257]},{"op":"move_to","coords":[0,3279]},{"op":"line_to","coords":[-173,177,-50,131]}]],"keys":["pmap:min_admin_level"],"values":[8],"tags":[]},{"name":"earth","version":2,"extent":4096,"features":[[{"op":"move_to","coords":[-80,-80]},{"op":"line_to","coords":[4256,0,0,4256,-4256,0]},{"op":"close_path"}]],"keys":[],"values":[],"tags":[]}]
blob decompress 704775 us
TS_MVTDECODE_OK
> 

`````

The demo currently doesnt display the raster or mvt tile, but all the preludes for that are in place.
