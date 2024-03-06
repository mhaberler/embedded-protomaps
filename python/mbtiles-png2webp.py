import sqlite3 
import io
from PIL import Image

def convert_png_to_webp(png_data):
    # Open PNG image from bytes
    with Image.open(io.BytesIO(png_data)) as img:
        webp_buffer = io.BytesIO()
        # https://pillow.readthedocs.io/en/stable/handbook/image-file-formats.html#webp
        # im.save(stream, format="WebP", lossless=True, quality=100, method=5)
        img.save(webp_buffer, format="WEBP", quality=90)
        webp_data = webp_buffer.getvalue()
    return webp_data

conn = sqlite3.connect("/Users/mah/Ballon/src/BalloonWare/mapping/embedded-maps/data/basemapat_Ortho16.mbtiles")
cursor = conn.cursor()
cursor.execute("SELECT rowid, tile_column, tile_row, zoom_level, tile_data FROM tiles")
data = cursor.fetchall()
for row in data:
    rowid, tile_column, tile_row, zoom_level, png_data = row
    webp_data = convert_png_to_webp(png_data)
    
    try:
        cursor.execute("UPDATE tiles SET tile_data=? WHERE rowid=?", (webp_data,rowid))
    except sqlite3.Error as error:
        print("Failed to update sqlite table", error)
