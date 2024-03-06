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
