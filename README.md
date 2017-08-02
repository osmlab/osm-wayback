# osm-tag-history [![Build Status](https://travis-ci.org/mapbox/osm-tag-history.svg?branch=master)](https://travis-ci.org/mapbox/osm-tag-history)
Stores a RocksDB with OSM ID &lt;-> tag history and augments GeoJSON files with the history of tag changes.

## Build

If you need to source compile then you'll need [`libosmium`](https://github.com/osmcode/libosmium) and [`rocksdb`](https://github.com/facebook/rocksdb/blob/master/INSTALL.md) headers available. Then you build the program with `cmake`:

```
mkdir build
cd build
cmake ..
make
```

## Run

First build up a historic tag index.

```
build_tag_lookup INDEX_DIR OSM_HISTORY_FILE
```

Pass a stream of GeoJSON features as produced by [minjur](https://github.com/mapbox/minjur).

```
cat features.geojson | add_tags INDEX_DIR
```


## To consider if running against the planet: 

As of August, 2017:
 - `minjur --polygons planet-latest.osm.pbf > planet.geojson` => planet.geojson ~250 GB with 545 Million features.
