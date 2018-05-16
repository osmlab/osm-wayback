  var _ = require('lodash');

const MINOR_VERSION_SECOND_THRESHOLD = 60*15; //15 minutes
const DEBUG = true;

module.exports = function(osmObject){

  //Attributes of given OSM element and all possible versions
  this.nodeLocations = osmObject.nodeLocations;
  this.versions      = osmObject.history;
  this.osmID         = osmObject.osmID;

  //A map of potential historical geometries available at this point
  this.historicalGeometries = {};

  /**
   *  Heavy-lifting helper function that looks up nodes
   *  that are possibly part of any geometry.
   *
   *  Expects:
   *  {
   *     nodeRef    : the ID of the given node
   *     validSince : the earliest date that this geometry is valid (optional)
   *     validUntil : the latest date this geometry is valid (optional)
   *     changeset  : the changeset of the MAJOR geometry (safety case)
   *  }
  */
  this.getNodeVersions = function(args){
    var nodeRef    = args.nodeRef
    var validSince = args.validSince
    var validUntil = args.validUntil
    var changeset  = args.changeset

    var that = this;

    //Look up this node in nodeLocations
    nodeVersionsByChangeset = that.nodeLocations[nodeRef.toString()];

    if (nodeVersionsByChangeset==undefined){
      console.error("No version of Node");
      return null;
    }

    // Ensure node versions are sorted by time
    var nodeVersions = _.sortBy(Object.values(nodeVersionsByChangeset),function(n){return n.t});

    //Filter to only have geometries... (not representing deleted geometries atm)
    nodeVersions = nodeVersions.filter(function(n){return n.hasOwnProperty('p')})

    //If there's only one version of the node, use it (always)
    if (nodeVersions.length==1){ return nodeVersions; }//

    //Be prepared to fall through and return the closest node less than validSince.
    var prevNode = nodeVersions[0]; //First version of the node
    var filteredNodes = []

    if(validSince){
      //Cut out previous versions, but save t-1
      nodeVersions.forEach(function(node){

        // OVERRIDE 1: Always add if changeset matches (the timestamp will likely be less, but changeset will match)
        if (node.c==changeset){
          filteredNodes.push(node)
        }else if (node.t >= validSince){
          filteredNodes.push(node)
        }

        prevNode = node;
      })
      //Safety condition 1: If there are now NO NODES, return prevNode
      if(filteredNodes.length==0){
        return [prevNode];
      }
    }else{
      filteredNodes = nodeVersions;
    }

    //filteredNodes now only has nodes greater than given timestamp;

    //stay safe from atomic changes below...
    var filterable = JSON.parse(JSON.stringify(filteredNodes));

    //If we have a validUntil, then filter the future nodes out
    if (validUntil){

      //OVERRIDE 2: IF there is a matching changeset, be sure it doesn't get abandoned
      filterable = filterable.filter(function(v){return (v.t < validUntil || v.c==changeset)})

      if(filterable.length==0){
        //If this removed all nodes, then return the most recent version; there is likely a _deleted_ version later
        return [prevNode]
      }
    }
    if (filterable.length==1){
      return filterable; //Only 1 possible case, return it
    }else{
      //OVER RIDE 2: If there is not an actual geometry change, then don't return it!
      try{
        var prev = filterable[0].p
        var diffGeoms = [filteredNodes[0]];

        for(var i=1;i<filterable.length;i++){

          if (prev[0]!=filterable[i].p[0] || prev[1]!=filterable[i].p[1]){
            diffGeoms.push(filterable[i])
            prev = filterable[i].p
          }
        }
        return diffGeoms;
      }catch(e){
        console.warn("\nNODE: "+nodeRef)
        console.warn("PREV")
        console.warn(prev)
        console.warn("FILTERABLE")
        console.warn(filterable)
        throw e
      }
    }

    //If we get to this condition, then there are no possible nodes to satisfy the condition
    throw "No Possible Nodes"
    return false
  }


  /**
   *  Given a major version, build all possible geometries
   *
   *  Expects:
   *  {
   *     nodeRefs   : The list of nodes associated with this MAJOR version
   *     validSince : the earliest date that the Major version is valid (optional)
   *     validUntil : the latest date that the Minor version is valid (optional)
   *     majorVersionChangeset  : the changeset of the MAJOR geometry
   *  }
  */
  this.buildAllPossibleVersionGeometries = function(args){

    var validUntil = args.validUntil;
    var validSince = args.validSince;
    var changeset  = args.majorVersionChangeset;

    var that = this;

    if (DEBUG){
      console.warn(`  Building all possible geometries between ${validSince} - ${validUntil}`)
    }

    var majorVersion  = []; //There should only ever be 1 major version?
    var minorVersions = [];

    var aPossibleGeometry = [];

    //Iterate through the nodes associated with this version
    args.nodeRefs.forEach(function(nodeRef){

      //Look up all possible versions of the node
      var possibleNodes = that.getNodeVersions({
        nodeRef: nodeRef,
        validSince: validSince,
        validUntil: validUntil,
        changeset: changeset});

      if( possibleNodes ){
        majorVersion.push(Object.assign({}, possibleNodes[0]));

        aPossibleGeometry.push(Object.assign({},possibleNodes[0]));

        //If there are still nodes, then we have a new minorVersion
        if (possibleNodes.length>1){

          minorVersions.push(aPossibleGeometry)

          //Start at this new version, and create new possible versions
          for(var i=1; i<possibleNodes.length; i++){
            console.warn('Minor Version!')
          }

          //TODO: Build all possible minor versions
          // aPossibleGeometry = JSON.parse(JSON.stringify(majorVersion));
          // aPossibleGeometry.pop()//remove that node, add the fancy new one
          // aPossibleGeometry.push(possibleNodes[1])
          //There are more possibilities!
          // console.log('minor version possible')
          minorVersions.push("minorVs")
        }
      }else{
        throw "NO NODES RETURNED by getNodeVersions() for " + nodeRef
      }
    })

    return {
      majorVersion: majorVersion.map(function(g){return g.p}),
      minorVersions: minorVersions
    }
  }


  /**
   *  Iterate through an OSM object's history and construct all possible geometries
   *
   *  Expects: Nothing, call on Object.
   *
   *  Returns: Nothing, populates the ``historicalGeometries`` attribute.
  */
  this.buildGeometries = function(){
    var that = this;
    var validSince, validUntil;

    if(DEBUG){
      console.warn(`\n\nReconstructing Geometries for ID: ${that.osmID}\n==================`)
    }

    //Versions is an object's history, length should correspond to current 'v'
    for(var i=0; i<that.versions.length; i++){
      validSince = false;
      validUntil = false;

      if(DEBUG){
        console.warn(`  Going for Major Version: ${that.versions[i].i} with timestamp ${that.versions[i].t} by ${that.versions[i].h} c:${that.versions[i].c}`)
      }

      //If it's not the first entry, there is a previous major version that covers these changes, so only worry about changes since this version came to be.
      if(i>0){
        validSince = that.versions[i].t
      }
      // //If there's another version to come, set validUntil to the next version
      if(i < that.versions.length-1){
        validUntil = that.versions[i+1].t
      }

      //Now construct all possible geometries for this Major Version:
      //Breaking Case: If it's not 'visible' and version has no nodes, then don't try to create a geometry...
      if( that.versions[i].hasOwnProperty('n') ){
        //Construct all possible geometries for this version, based on the nodeRefs.

        var geometries = that.buildAllPossibleVersionGeometries({
          nodeRefs: that.versions[i].n,
          validSince: validSince,
          validUntil: validUntil,
          changeset: that.versions[i].c
        })

        if(geometries.majorVersion){
          that.historicalGeometries[i] = [{
            'type':"Feature",
            'properties':{
              '@version': i,
              '@minorVersion': 0,
              '@validSince': that.versions[i].t,
              '@validUntil': (i<that.versions.length-1)? that.versions[i+1].t: null
            },
            'geometry': {
              'type':"LineString",
              'coordinates' : geometries.majorVersion
            }
          }]
        }

        if (geometries.minorVersions){
          //still gotta figure this part out.
        }
      }

      if(DEBUG){
        console.warn(`Major Version: ${that.versions[i].i} has ${geometries.minorVersions.length} minor versions`)
      }
    }
  }
}
