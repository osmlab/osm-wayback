# osm-wayback
<!-- [![Build Status](https://travis-ci.org/mapbox/osm-wayback.svg?branch=master)](https://travis-ci.org/mapbox/osm-tag-history) -->

Creates a [RocksDB](//rocksdb.org) key-value store of each version of OSM objects found in OSM history files. This history index can then be used to augment GeoJSON files of OSM objects to add a `@history` property that includes a record of all _previous_ edits.

#### Notes:

1. The history is index is keyed by `osm-id`+"!"+`version` (with separate column families for nodes, ways, and relations).

2. `add_history` will lookup every previous version of an object passed into it. If an object is passed in at version 3, it will look up versions 1,2, and 3. This is necessary for the tag comparisons. In the event there exists a version 4 in the index, it will not be included because version 3 was fed into `add_history`.

3. Since `add_history` is driven by a stream of (current, valid) GeoJSON objects, deleted objects are not yet supported.

## Build

Install mason to manage dependencies

```
git clone --depth 1 --branch v0.14.1 https://github.com/mapbox/mason.git .mason
```

Then build with `cmake`:
```
mkdir build
cd build
cmake ..
make
```

## Run

First build up a historic lookup index.
**Note:** For large files (Country / Planet), increase `ulimit` so that RocksDB can have many files open at once (>4000 for the full planet history file).

```
build_lookup_index INDEX_DIR OSM_HISTORY_FILE
```

Second, pass a stream of GeoJSON features as produced by [osmium-export](http://docs.osmcode.org/osmium/latest/osmium-export.html) to the `add_history` function

```
cat features.geojsonseq | add_history INDEX_DIR
```

The output is a stream of augmented GeoJSON features with an additional `@history` array of the following schema.


## Performance
This program essentially converts an OSM history file into a RocksDB Key-Value store for _very fast_ lookups. As such, it requires a lot of disk space, but not much memory. A few statistics are below to help gauge the amount of disk space required.

In general, RocksDB indexes will be 10-15x the size of the input history file. Full planet indexes can be near 1TB.


Timewise, here are some rough estimates, these were run  locally on a 2013 Macbook Air, i7.

`build_lookup_index`

| .osh.pbf file | Nodes | INDEX   | ~ Time (seconds)|
|---------------|-------|---------|-----------------|
| ~ 7 MB        | 0.5M  | ~ 50 MB |  1           |
| ~ 50 MB       | 5M    | ~ 650 MB|  13
| ~ 280 MB      | 30M   | ~ 3.5 GB|  70          |


<hr>
`add_history`

| GeoJSON Features | Additional history<br>versions added| Time (seconds)|
|------------------|---------------------|---------------|
| 56k              | 95k                 | 4             |
| 820k             | 973k                | 57            |

_Estimate ~ 1M input features per minute?_

(For reference, there are about ~600M GeoJSON features in the daily planet file).

## Historic Geometries
A fourth column family storing node locations can be created during `build_lookup_index`, depending on the value of the variable, `LOC` in `build_lookup_index.cpp`.

If the node location column family exists, the <HISTORY GEOJSONSEQ> may be passed to `add_geometry`. This function looks up every version of every node in each historical version of the object. It adds `nodeLocations` as a top-level dictionary, keyed by `node ID` and then `changeset ID` for each node.

```
cat <HISTORY GEOJSONSEQ> | add_geometry <ROCKSDB> > <HISTORY GEOJSONSEQ with Node Locations>
```

From here, this stream is passed into `geometry-prototyping/index.js` to reconstruct major and minor geometry versions for every object. This can be done in one step with the following command:

```
cat <HISTORY GEOJSONSEQ> | ../build/add_geometry <ROCKSDB> | node index.js > <HISTORY GEOJSONSEQ with Geometry>
```

See [geometry-prototyping/README.md](geometry-prototyping/README.md) for more information on which options are available and the description fo the schema in [HISTORICAL_SCHEMA_V1.md](HISTORICAL_SCHEMA_V1.md) for the pros and cons of each schema.

Currently, 3 output types are supported:
1. Every major and minor version are independent objects (Best for rendering historical geometries)
2. Entries in the `@history` object include `geometry` attribute (Best for historical analysis)
3. The `@history` object is a TopoJSON object, storing every version of the object. (More efficient than 2.)
