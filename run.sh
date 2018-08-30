#!/bin/bash
clear
echo ""
echo "Running OSM-Wayback for history file:"
echo "--Input history file: $1"
echo "--Base file name:     $2"
echo ""
echo "Preparation Step 1: osmium time-filter"
mason_packages/.link/bin/osmium time-filter --overwrite -o $2.osm.pbf $1
echo "Preparation Step 2: osmium export"
mason_packages/.link/bin/osmium export -c ~/osm-qa-tiles/osm-qa-tile.osmiumconfig -f geojsonseq $2.osm.pbf > $2.geojsonseq

echo ""
echo "Now beginning OSM-Wayback"
echo "-------------------------"
echo "Commands that excecute are denoted with (*)"
echo ""
echo "==================================================="
echo "|| Step 1: Create Lookup Index from history file ||"
echo "==================================================="
echo ""
echo "* build_lookup_index $2_INDEX $1"
time build/build_lookup_index $2_INDEX $1

echo""
echo "=============================================="
echo "|| Step 2: Feed GeoJSONSeq into add_history ||"
echo "=============================================="
echo ""
echo "* cat $2.geojsonseq | build/add_history $2_INDEX > $2.history"
time cat $2.geojsonseq | build/add_history $2_INDEX > $2.history

echo ""
echo "================================================"
echo "|| Step 3: Enrich history file with locations ||"
echo "================================================"
echo ""
echo "* cat $2.history | build/add_geometry $2_INDEX > $2.history.geometries"
echo ""
time cat $2.history | build/add_geometry $2_INDEX > $2.history.geometries

echo ""
echo "======================================================="
echo "|| Step 4: Create TopoJSON Histories from Geometries ||"
echo "======================================================="
echo ""
echo "* cat $2.history.geometries | node geometry-prototyping/index.js > $2_historical_geometries_topojson.geojsonseq"
time cat $2.history.geometries | node geometry-prototyping/index.js > $2_historical_geometries_topojson.geojsonseq

echo ""
echo "==============================================="
echo "|| Step 5: Run geojsonseq through tippecanoe ||"
echo "==============================================="
echo ""
echo "* tippecanoe -Pf -pf -pk -ps -Z15 -z15 --no-tile-stats --no-duplication -o $2_historical.mbtiles -l historical_topojson $2_historical_geometries_topojson.geojsonseq"
time tippecanoe -Pf -pf -pk -ps -Z15 -z15 --no-tile-stats --no-duplication -o $2_historical.mbtiles -l historical_topojson $2_historical_geometries_topojson.geojsonseq
