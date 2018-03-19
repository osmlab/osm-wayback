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

## Historical Feature Schema for TAGS
OSM objects that have history will have an extra attribute prefixed with `@`. This ``@history` object is an array of individual historical versions and is stored in the properties of the main GeoJSON Feature. The final object in the history array is the current version of the object. This allows tag changes to be easily tracked between all versions.
```
"@history": [
  <history object version 1>,
  <history object version 2>,
  ...
  <current version of object>
 ]
 ```
It should be noted that not all versions of an object may be included. Relying on solely the version number is not a guarantee. Previous versions may be missing for a variety of reasons including redaction, deletion of all tags, etc.

### History Objects

Similar to the standard OSM-QA tile, each of these standard OSM object properties are kept.
```
@version
@changeset
@timestamp
@uid
@user
```

### Additional Attributes
#### 1. Tags

Tags are removed from the historical versions objects and only the diffs recorded. While this will require iterating over the tags in history to reconstruct them exactly per version, it has two worthy benefits: 1) Easily see when tags were added/changed/deleted and 2) limited duplication of data.

There are four possible outcomes when comparing two versions:

1. **Nothing** is changed on the tags, the change was strictly geometrical. This is not possible with a WAY. A change to a way's geometry will not increment the version of the way.
1. **New Tags** (`new_tags`): A user adds new tags to the object.
1. **Delete Tags** (`del_tags`): Tags are removed from one version to the next.
1. **Modify Tags** (`mod_tags`): The value of tag(s) are changed. Name expansion, for example: `Elm St.` --> `Elm Street`. In this case, we'll record both the previous value and the new value with this version so that the change can be easily referenced without looking at previous versions.
```
"@new_tags" : {
  "key1" : "value1",
  "key2" : "value2"
},
"@del_tags": {
   "previous key 1" : "previous value 1",
   "previous key 2" : "previous value 2"
},
"@mod_tags": {
  "key1" : [
    "previous value",
    "new value"
  ],
  "key2" : [
    "previous value 2",
    "new value 2"
  ]
}
```


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