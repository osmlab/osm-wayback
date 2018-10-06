Reconstructing Historical Geometries
====================================

majorVersion vs. minorVersion
-----------------------------
Whenever an OSM object is edited, the version number is incremented. This number keeps a record of how many times the attributes of an object have been changed. This number does not, however, count geometry-only changes. Since individual nodes have no reference to any ways that may contain them,

Nodes
-----
OSM-Wayback stores locations with nodes themselves so retrieving their geometries is afforded through `add_history`. To create valid geometries (or TopoJSON), you can feed this output directly into `node index.js`. For ways, you must run `add_geometry` first to build the `nodeLocations` object.


Ways
----
The function `add_geometry` adds a `nodeLocations` object to each way that includes all of the location histories for each of the nodes that the way has ever referenced.

For now, `add_geometry` does not actually process these geometries, it simply adds them to the objects.

To understand possible histories of ways, break them down into specific cases:

### Versions
There are 3 possible cases regarding a (major) version of a way. It is either (1) the first version of the way, (2) an edited version, but not the most recent, or (3) is the most recent version.

##### Case 1: It is the first version
The timestamp of the subsequent version is the latest potential cut-off for this major version, this becomes the `validUntil` time. Any  referenced nodes edited before this `validUntil` create potential minor versions.

##### Case 2: A previous version and a next version
The timestamp of the major version becomes `validSince` and then the same process for the first version is followed; the timestamp of the subsequent edit becomes `validUntil` and any nodes edited between these two times create potential minor versions.

##### Case 3: This is the most recent version of the way
There is no `validUntil` attribute, it is currently valid. `validSince` is the timestamp of this major version. Any referenced nodes edited after `validUntil` create potential minor versions.

Using these `validSince` and `validUntil` timestamps, we lookup any edits to referenced nodes between these times. Each time one of these nodes is changed, a new potential minor version of the geometry exists. We only consider changes to the node coordinates as real changes.

With the list of changed nodes, we rebuild every possible version of the way. If the number of potential versions is too large, it keeps the first, second, and last minor version for every major version.
