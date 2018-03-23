Reconstructing Historical Geometries
====================================

This is a notoriously hard problem that partially explains _why_ this has not been done before.


TODO: Explain version vs. minorVersion


Nodes
-----
OSM-Wayback stores locations with nodes themselves so reconstructing their geometries is afforded through `add_history`


Ways
----
To understand possible histories of ways, break them down into specific cases

### Versions
There can only ever exist three cases regarding a version of a way: It is either (1) the first version (and there is no previous version), (2) there is a previous version and a next version, or (3) it is the most recent version.

### Case 1: It is the first version


#### Case 1: A previous version
version i+1 then has a timestamp. known as `t_validUntil`, at this time, this version is no longer valid. Associated with each version is a list of nodes that go with this version. With these two pieces of information, we can get a list of all possible geometries for this particular major version.

`allPossibleGeometries = construct_all_possible_geometries(node_refs, t_validUntil)`

There are now two more possible cases: 1 or more possible geometries

```
if (allPossibleGeometries.length > 1) {
  // There are multiple possible minorVersions.
  // For each minorVersion, check the timestamps and the userID associated with it
  // TODO: Could using the changeset be more efficient?

  for ( possibleMinorVersion in allPossibleGeometries ){
    //Timestamp?

    //

  }
}else if ( allPossibleGeometries.length == 1) {
  return {
	geometry:     allPossibleGeometries[0],
        minorVersion: 0
  }
}else{
  //Something failed, no posible geometries.
}
```

### Case 2: This is the most recent version of the way

The first version of a way 
