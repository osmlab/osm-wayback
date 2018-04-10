Historical OSM Analytics Schema
===============================
#### For Tile-Based Analysis

How do we best represent the OSM editing history in an analyst-friendly format to promote scalable, reproducible, and rigorous OSM research? Described here are two schemas for representing OSM editing histories that are optimized for (1) analysis and (2) rendering. 

These are based off [osm-qa-tiles](http://www.osmlab.github.io/osm-qa-tiles) and are described as properties of GeoJSON features, with the intent of being encoded to vector tiles for both analysis (tile-reduce) and visualization (namely mapbox-gl).

Broadly, there are two possible ways to represent historical OSM data:

#### 1. _Embedded History_
In this schema, objects are unique OSM objects. This is similar to how objects are treated on [openstreetmap.org](http://www.openstreetmap.org): organized first by ID, then sorted by version if you [look up an object's history](https://www.openstreetmap.org/way/238241022/history).

Historical versions (or records of changes between versions) of an object are embedded into the object as an additional attribute, such as an additional `@history` property. This is the approach that [OSM-Wayback](http://www.github.com/osmlab/osm-wayback) currently takes to track attribute (tag) changes overtime.

**Pros**

- Unique, Distinct Objects
- Easy to record changes between versions of objects such as _deleted tags_, _modified tags_, or _added tags_.
- Limits duplication of data

**Cons**

- Each object's `@history` needs to be processed to learn about the object at a certain point in time.
- Difficult to render for an arbitrary point in time.

#### 2. _Distinct Historical Versions_
In this schema, each unique version of an object exists as its own object. Included with each object are two additional top-level properties representing the `time this version was created` and the `time this object was next updated` (when this version is no longer the latest).

In this schema, data may be duplicated (or referenced multiple times) so that individual versions of objects each include a complete set of valid attributes.

**Pros**

- Optimized for rendering objects as they exist at any point time.
- No parsing a `history` object required to obtain a specific version in time.

**Cons**

- How objects change between versions is difficult to calculate


**Note:** It is entirely feasible to convert between these two schemas without any external information sources. Each of these are lossless encodings of an object's history.


A third form does not capture the editing histories, but shows instead how the map looked at a certain, static point in time: 

##### (3.) _Historical Snapshots_
Historical snapshots, like [historical osm-qa-tiles](http://osmlab.github.io/osm-qa-tiles/historic.html) represent the data as it existed at any given point in time. These can be used for time-series analysis at whatever resolution the snapshots are available (currently quarterly).

This schema is good only for single-point-in-time analysis and includes no record of how objects change overtime. For example, we can use a hsitorical snapshot to answer "how many kilometers of roads 


### Geometries
Given the node/way/relation topology of OSM, these 

See geometry-prototyping/README.md



Schema 1: Embedded Histories
============================

OSM objects that have history will have an extra attribute, `@history`. This history object is an array of individual historical versions and is stored in the properties of the main GeoJSON Feature. 
The final object in the history array is the current version of the object. This allows tag changes to be easily tracked between all versions.

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

Similar to the standard OSM-QA tile, each of the standard OSM object properties are recorded, but they are simplified to save space in the final JSON. This simplification reduces final file sizes by up to 10%.

```
@version   --> i  // (iteration)
@changeset --> c
@timestamp --> t
@uid       --> u
@user      --> h  // (handle)
```

### Additional Attributes
#### 1. Tags

Tags are removed from the historical versions objects and only the diffs recorded. While this will require iterating over the tags in history to reconstruct them exactly per version, it has two worthy benefits: 1) Easily see when tags were added/changed/deleted and 2) limited duplication of data.

For schema simplification, tags can be thought of as "attributes" and represented by the key "a".

There are four possible outcomes when comparing two versions:

1. **Nothing** is changed regarding attributes.
1. **attributes Added** (`aA`): A user adds a new tag to the object.
1. **attributes Deleted** (`aD`): Tags are removed from one version to the next.
1. **attributes Modified** (`aM`): The value of attribute(s) are changed. Name expansion, for example: `Elm St.` --> `Elm Street`. In this case, we record both the previous value and the new value with this version so that the change can be easily referenced without looking at previous versions.

```
"aA" : {
  "key1" : "value1",
  "key2" : "value2"
},
"aD": {
   "previous key 1" : "previous value 1",
   "previous key 2" : "previous value 2"
},
"aM": {
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



Schema 2: Distinct Historical Versions
======================================


