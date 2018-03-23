Historical OSM Analytics Schema (for Tile-Based Analysis)
=========================================================

How do we best represent the OSM editing history in an analyst-friendly format to promote scalable, reproducible, and rigorous OSM research?

Building from the OSM-QA-Tile format, the approach taken here is to embed an object's editing history as an additional `@history` attribute in the object's properties. Then, when processing a tile with a tool like `tile-reduce`, you can (first Parse) this array and then see how the object changed throughout versions.





Historical Geometries
=====================
Given the topological data structure of ways --> nodes, geometry only changes to ways are not always captured in the major version of a way.

Incremental versioning with `geom_version` or `minorVersion` tags are a method of tracking individual geometries.