# osm-wayback [![Build Status](https://travis-ci.org/mapbox/osm-wayback.svg?branch=master)](https://travis-ci.org/mapbox/osm-tag-history)
Stores a RocksDB with OSM ID &lt;-> tag history and augments GeoJSON files with the history of tag changes.

:rocket: Final goal is to create historic geometries for each intermediate version of an OSM feature.

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

First build up a historic tag index.

```
build_tag_lookup INDEX_DIR OSM_HISTORY_FILE
```

Pass a stream of GeoJSON features as produced by [minjur](https://github.com/mapbox/minjur) (Note: minjur is no longer being developed, use [osmium-export](http://docs.osmcode.org/osmium/latest/osmium-export.html) moving forward).

```
cat features.geojson | add_tags INDEX_DIR
```

The output is a stream of augmented GeoJSON features with an additional `@history` array of the following schema.

## Historical Feature Schema for TAGS
OSM objects that have history will have an extra attribute prefixed with `@`. This @history object is an array of individual historical versions and is stored in the properties of the main GeoJSON Feature. The final object in the history array is the current version of the object. This allows tag changes to be easily tracked between all versions.
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
