# osm-wayback
<!-- [![Build Status](https://travis-ci.org/mapbox/osm-wayback.svg?branch=master)](https://travis-ci.org/mapbox/osm-tag-history) -->

Creates a [RocksDB](//rocksdb.org) key-value store of each version of OSM objects found in OSM history files. This history index can then be used to augment GeoJSON files of OSM objects to add a `@history` property that includes a record of all _previous_ edits.

osm-wayback is currently designed to support large(ish)-scale historical analysis of OpenStreetMap edits, specifically focused on how objects change overtime (and who is editing them).

#### Current Development Notes:

1. The history is index is keyed by `osm-id`+"!"+`version` (with separate column families for nodes, ways, and relations).

2. `add_history` will lookup every previous version of an object passed into it. If an object is passed in at version 3, it will look up versions 1,2, and 3. This is necessary for the tag comparisons. In the event there exists a version 4 in the index, it will not be included because version 3 was fed into `add_history`.

3. Since `add_history` is driven by a stream of (current, valid) GeoJSON objects, deleted objects are not yet supported.

## Build

Install mason to manage dependencies


	git submodule init
	git submodule update
	
Then build with `cmake`:

	mkdir build
	cd build
	cmake ..
	make
	
To use the `run.sh` script, also run the following:

	.mason/mason install osmium 1.9.1
	.mason/mason link osmium 1.9.1
	
	.mason/mason install tippecanoe 1.31.0
	.mason/mason link tippecanoe 1.31.0
	
	cd geometry-reconstruction
	npm install


## Running

#### A canned workflow: `run.sh`
The `run.sh` script automates all of the steps to turn OSM history files into historical vector tiles with only 2 inputs: `OSM_HISTORY_FILE` and `ROOT_FOR_OUTPUT`.
	
For example, to run generate historical vector tiles from the albany example file included in `example/history_of_albandy.osh.pbf`: 
	
	$ ./run.sh example/history_of_albany.osh.pbf example/albany

This will create the following files in the `example` directory (in the following order): 

| File | Description|
|------------------------------|------------------------------|
| **albany.osm.pbf** | Latest version of (all) objects in `history_of_albany.osh.pbf`|
| **albany.geojsonseq** | GeoJSON sequence of objects exported by _osmium export_ with the `example/osmiumconfig` configuration. _(Not ALL OSM objects, only what osmium understands)_| 
| **albany_INDEX** | The RocksDB Index of `history_of_albany.osh.pbf` |
| **albany.history**| Each OSM object from `albany.geojsonseq` with an additional `@history` property that contains each previous (major) version (see [`HISTORICAL_SCHEMA.md`](https://github.com/osmlab/osm-wayback/blob/master/HISTORICAL_SCHEMA.md) for more on this schema) | 
| **albany.history.geometries** | Each feature from `albany.history` enriched with an additional `nodeLocations` attribute storing the location of every version of every node ever associated with each object.  |
| **albany\_historical\_geometries_<br>topojson.geojsonseq** | Each feature from `albany.history.geometries` with a TopoJSON encoded `@history` attribute that describes each historical version (including minor versions) with geometries|
| **`albany_historical.mbtiles`** | Historical vector tiles rendered at zoom 15 for albany! |

_Note that once run, each of these files are standalone and can be deleted in the order they are generated. (Each file is used only as the input to the next function. This workflow is a function of each utility here being standalone to provide options: You could build a North-America INDEX and then run new_york.geojsonseq against it at the same time as running san_francisco.geojsonseq. Including geometries requires a second pass once histories are looked up. This is not done in one pass (and time costs are negligible) to allow only tag-history analysis)_


#### The complete workflow

First build up a historic lookup index.
**Note:** For large files (Country / Planet), increase `ulimit` so that RocksDB can have many files open at once (>4000 for the full planet history file).

	build_lookup_index INDEX_DIR OSM_HISTORY_FILE

Second, pass a stream of GeoJSON features as produced by [osmium-export](http://docs.osmcode.org/osmium/latest/osmium-export.html) to the `add_history` function


	cat features.geojsonseq | add_history INDEX_DIR

The output is a stream of augmented GeoJSON features with an additional `@history` array (see [HISTORICAL_SCHEMA.md](https://github.com/osmlab/osm-wayback/blob/master/HISTORICAL_SCHEMA.md)) for more on the schema of `@history`. Note: If a feature is not in the input file, it's history will not be in the output file.


## Historic Geometries
A fourth column family storing node locations can be created during `build_lookup_index`, depending on the value of the variable, `LOC` in `build_lookup_index.cpp`.

If the node location column family exists, the `HISTORY GEOJSONSEQ` may be passed to `add_geometry`. This function looks up every version of every node in each historical version of the object. It adds `nodeLocations` as a top-level dictionary, keyed by `node ID` and then `changeset ID` for each node.

	cat <HISTORY GEOJSONSEQ> | add_geometry <ROCKSDB> 
	
Will create a line-delimited stream of GeoJSON OSM objects with the `nodeLocations` attribute.

Reconstructing historical geometries (available for nodes & ways) is then done in a separate process in `geometry-reconstruction`:

	node geometry-reconstruction/index.js <HISTORY GEOJSONSEQ with Node Locations>  
	
Currently, multiple output types are supported, see [`geometry-reconstruction/README.md`](https://github.com/osmlab/osm-wayback/blob/master/geometry-reconstruction/README.md) for more information about the following output types:

1. Every major and minor version are independent objects (Best for rendering historical geometries)
2. Entries in the `@history` object include `geometry` attribute (Best for historical analysis)
3. The `@history` object is a TopoJSON object, storing every version of the object. (More efficient than 2.)
