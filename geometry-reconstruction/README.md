# Historical Geometry Reconstruction

Uses [json-stream-reduce](https://github.com/jenningsanderson/stream-reduce), a fork of tile-reduce that works on large files of line-delimited JSON data to reconstruct historical geometries (both major and minor versions) of OSM objects from JSON input that includes historical node locations.

## Example

	npm install
	node index.js <Line-delimited GeoJSON file with @history and nodeLocations attributes>

This will create a stream of new GeoJSON objects in one of the following formats.

## Available Output Formats

| Type | Description | Recommended Use |
|------|-------------|-----------------|
| Distinct objects with [TopoJSON](https://github.com/topojson/topojson) encoded `@history` | Each line is an individual OSM object. The histories of each object are encoded (with both minor and major version geometries) as TopoJSON for space-efficiency | Tiling & subsequent analysis with tile-reduce
| All versions |  Every version (minor and major) of every object with `validSince` and `validUntil` timestamps | Visualizing change overtime
| Distinct objects with full history| `@history` object as JSON array. Should always use TopoJSON encoding of `@history` object unless there is a problem with the encoding. | Debugging |

For each of these outputs, the amount of metadata can be specified. For example, including only the deltas between versions encoded as `aA`, `aM`, and `aD` for attributes added, attributes modified, and attributes deleted, respectively. These options are set in the `CONFIG` variable in _map-geom-reconstruction.js_.

### Limitations
`element-reconstruction/` includes each of the history reconstructors for each OSM element type. `relation-history-builder.js` is currently a stub that does not recreate any historical geometries for any relations.

