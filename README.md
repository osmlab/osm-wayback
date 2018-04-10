# osm-wayback
<!-- [![Build Status](https://travis-ci.org/mapbox/osm-wayback.svg?branch=master)](https://travis-ci.org/mapbox/osm-tag-history) -->

Creates a [RocksDB](//rocksdb.org) key-value store of each version of OSM objects found in OSM history files. This history index can then be used to augment GeoJSON files of OSM objects to add a `@history` property that includes a record of all _previous_ edits.

:rocket: Final goal is to create historic geometries for each intermediate version of an OSM feature.

#### Notes:

1. The history is index is keyed by `osm-id`+"!"+`version` (with separate column families for nodes, ways, and relations).

2. `add_history` will lookup every previous version of an object passed into it. If an object is passed in at version 3, it will look up versions 1,2, and 3. This is necessary for the tag comparisons. In the event there exists a version 4 in the index, it will not be included because version 3 was fed into `add_history`.

3. Since `add_history` is driven by a stream of GeoJSON objects, deleted objects are not supported.

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
cat features.geojson | add_history INDEX_DIR
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