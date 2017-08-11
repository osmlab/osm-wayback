# osm-tag-history [![Build Status](https://travis-ci.org/mapbox/osm-tag-history.svg?branch=master)](https://travis-ci.org/mapbox/osm-tag-history)
Stores a RocksDB with OSM ID &lt;-> tag history and augments GeoJSON files with the history of tag changes.

## Build


Install mason to manage dependencies

```
git clone --depth 1 --branch v0.14.1 https://github.com/mapbox/mason.git .mason
```

Then build with c`make`:
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

The output is a stream of augmented GeoJSON features with an additional `@history` array of the following schema:

# Historical Feature Schema for TAGS
Version 1 from [osm-analysis-collab](https://github.com/mapbox/osm-analysis-collab/issues/30); Propose moving this discussion to this repository, perhaps a `TagHistorySchema_v1.md` document to track development.

OSM objects that have history will have an extra attribute prefixed with `@`, like the standard OSM properties from `minjur`. This @history object is an array of individual historical versions and is stored in the properties of the main GeoJSON Feature. The final object in the history array is the current version of the object. This allows tag changes to be easily tracked between all versions.
```
"@history": [ 
  <history object version 1>,
  <history object version 2>,
  ...
  <current version of object>
 ]
 ```
It should be noted that not all versions of an object may be included. Relying on solely the version number is not a guarantee. This is mostly due to the redaction period, but also partly remains a mystery.

### History Objects

Similar to the standard OSM-QA tile, each of these standard OSM object properties are kept.
```
@version
@changeset
@timestamp
@uid
@user
Additional Attributes
```
#### 1. Tags

Tags should be removed from the objects and only the diffs recorded. While this will require iterating over the tags in history to reconstruct them exactly per version, it has two worthy benefits: 1) Easily see when tags were added/changed/deleted and 2) limited duplication of data.

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
