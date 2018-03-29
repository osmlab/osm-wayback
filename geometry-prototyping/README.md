Constructing Historic Geometries with RocksDB + NodeJS
======================================================

These scripts expand the JSON output from OSM-Wayback's `add_geometry` into actual GeoJSON geometries per version.

When finished, one should be able to run: 

`cat <HISTORY.geojsonseq> | add_geometry <ROCKSDB> | node index.js > <HISTORY WITH GEOMETRIES> `


See `GeometryReconstruction.md` for more detailed information on how these are generated per feature, but this module aims to produce two possible types of output:

### 1. Embedded History (geometries encoded as TopoJSON)
Each GeoJSON object represents a single OSM object, with the geometry of the most recent version. Where applicable, a TopoJSON object is encoded within the `@history` key that describes each historical version of the object. TopoJSON dramatically reduces the object's size by only storing coordinates once per object.

Historical analysis of OSM objects then parses the `@history` object as TopoJSON to get an ordered list of the geometry history.


### 2. Individual Features
Each version of an object is an individual object itself. Each object has a `validSince` and a `validUntil` timestamp that describes when it was the current version.

This format can be optimized for creating vector tiles that can easily draw the map at any point in time by filtering for the corresponding objects using `validSince` and `validUntil`.  

_Note_: Not filtering by these attributes and instead using a feature such as `@version` to determine Opacity will show density based editing / corrections.

Version to version comparisons of these features is difficult, however, because objects are independent of one another. To identify changes between versions, consider using the embedded history format.