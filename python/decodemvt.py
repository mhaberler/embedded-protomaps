import mapbox_vector_tile
import json

from mapbox_vector_tile import decode

# Specify the path to your OSM PBF file
osm_pbf_file = 'data/test.mvt'

# Read the binary data from the PBF file
with open(osm_pbf_file, 'rb') as file:
    pbf_data = file.read()

# Decode the PBF data using mapbox_vector_tile
js = decode(pbf_data)

# Print the decoded data (you might want to explore the structure of the decoded data)

print(json.dumps(js, indent=1))


