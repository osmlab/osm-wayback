var _ = require('lodash');

const MINOR_VERSION_SECOND_THRESHOLD = 60*15; //15 minutes
const CHANGESET_THRESHOLD            = 60*1   // 1 minute
const DEBUG = 0;

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
    console.error("No version of Node: "+nodeRef.toString());
    return null;
  }

  // Ensure node versions are sorted by time
  var nodeVersions = _.sortBy(Object.values(nodeVersionsByChangeset),function(n){return n.t});

  //Filter to only have geometries... (not representing deleted geometries atm)
  nodeVersions = nodeVersions.filter(function(n){return n.hasOwnProperty('p')})

  if (nodeVersions.length==0){
    return false;
    if(DEBUG){
      console.error("LIKELY REDACTED")
    }
  }

  //If there's only one version of the node, use it (always)
  if (nodeVersions.length==1){
    return nodeVersions;
  }

  //Always include the previous version of the node; especially within changeset length threshold
  var prevNode = nodeVersions[0]; //First version of the node
  var prevNodeNotAdded;
  var filteredNodes = []

  if(validSince){
    //Cut out previous versions, but save t-1
    nodeVersions.forEach(function(node){

      // OVERRIDE 1: Always add if changeset matches (the timestamp will likely be less, but changeset will match)
      if (node.c==changeset){
        filteredNodes.push(node)
      }else if (node.t >= validSince){
        filteredNodes.push(node)
      }else{
        //Not the same changeset and not >= validSince
        prevNodeNotAdded = Object.assign({},node);
      }

      prevNode = node;
    })
    //Safety condition 1: If there are now NO NODES, return prevNode
    if(filteredNodes.length==0){
      return [prevNode];
    }

    //If the first node in the list is TOO new, add prevNodeNotAdded
    if(prevNodeNotAdded){
      if(filteredNodes[0].t > validSince+CHANGESET_THRESHOLD){
        filteredNodes.unshift(prevNodeNotAdded)
      }
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
      if (prevNode){
        return [prevNode]
      }else{
        return false;
      }
    }
  }
  if (filterable.length==1){
    return filterable; //Only 1 possible case, return it
  }else{

    //OVER RIDE 2: If there aren't any different geometries, don't return it... too expensive
    try{
      var diffGeoms = [filterable[0]]; //basic case, just 1

      var prev = filterable[0].p

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
  // throw "No Possible Nodes"
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
  var changeset  = args.changeset;

  var that = this;

  if (DEBUG){
    console.warn(`  Building all possible geometries between ${validSince} - ${validUntil}`)
    console.warn(`  (${(new Date(validSince*1000)).toISOString()} - ${(new Date(validUntil*1000)).toISOString()})`)
  }

  var versions  = []

  //Iterate through the nodes associated with this version
  args.nodeRefs.forEach(function(nodeRef){

    //Look up all possible versions of the node
    var possibleNodes = that.getNodeVersions({
      nodeRef: nodeRef,
      validSince: validSince,
      validUntil: validUntil,
      changeset: changeset});

    //If nodes were returned, just add them to the array
    if( possibleNodes ){
      versions.push(possibleNodes.slice(0)) //Cloning this array
    }else{
      if(DEBUG){
        console.warn("NO NODES RETURNED by getNodeVersions() for " + nodeRef);
      }
    }
  })

  if(DEBUG){
    for(var i in versions){
      console.warn("--" + args.nodeRefs[i] + "---");
      versions[i].forEach(function(v_){
        console.warn(v_.i, v_.c, v_.t, v_.h)
      })
    }
  }

  //Take the first version
  var majorVersion = versions.map(function(a){return a[0]})

  if(DEBUG){
    console.warn("MAJOR VERSION: ")
    console.warn((new Date(_.max(majorVersion.map(function(n){return n.t}))*1000)).toISOString());
    console.warn(majorVersion.map(function(n){return n.h}).join(" > "))
  }
  var minorVersions;

  //Expand out the versions array
  var maxLen = _.max(versions.map(function(a){return a.length}))

  if(DEBUG){console.warn("\n" + maxLen + "\n")}

  if(maxLen>1){         //There are minor versions!
    minorVersions = [[]];

    //Iterate through each of the nodes, building geometries as they exist.
    for(var i=0; i<versions.length; i++){
      //If there is only 1 node, add it to ALL of the minorVersions;
      if(versions[i].length==1 ){
        for(var j=0; j<minorVersions.length; j++){
          minorVersions[j].push(versions[i][0])
        }
      }else{

        //minorVersions is going to grow with all permutations...
        var newPossibilities = [];

        //For each of the CURRENT minorVersions, add one of the new ones (within reason).
        for(var k=0; k<minorVersions.length; k++){

          // var lastTime = versions[i][0].t
          for(var j=0; j<versions[i].length; j++){
            //Reset baseGeom;
            var baseGeom = minorVersions[k].slice(0);

            //Always do it for the first version
            if (j<1){
              baseGeom.push(versions[i][j])
              newPossibilities.push(baseGeom)
            }else if (baseGeom.length < 5){
              //If the size of the geometry isn't insane
              baseGeom.push(versions[i][j])
              newPossibilities.push(baseGeom)

            }else if (k==minorVersions.length-1 && j==versions[i].length-1){
              //Just asdd the last one
              baseGeom.push(versions[i][j])
              newPossibilities.push(baseGeom)
            }
          }
        }
        //Now reset minorVersions to be newPossibilities;
        minorVersions = newPossibilities;
      }
    }

    //OKAY! NOW! How many are valid?
    mapped = []
    minorVersions.forEach(function(mV){
      var maxNode = mV[0];
      for(idx in mV){
        if (mV[idx].t > maxNode.t){
          maxNode = mV[idx]
        }
      }
      mapped.push([maxNode, mV])
    })

    minorVersions = undefined;

    sortedMinorVersions = _.sortBy(mapped, function(x){return x[0].t})

    if(DEBUG){
      console.warn("Minor Versions: ")
    }

    var countableMinorVersions = [];
    var prevTimestamp = sortedMinorVersions[0][0].t;
    var minorVersionIdx = 1;
    sortedMinorVersions.forEach(function(sorted){
      var mV = sorted[1]
      // console.warn("PREV TIMESTAMP: " + (new Date(prevTimestamp*1000)).toISOString())
      if (sorted[0].t > prevTimestamp + MINOR_VERSION_SECOND_THRESHOLD){
        if(DEBUG){
          console.warn(mV.map(function(n){return n.h}).join(" > "))
          console.warn(sorted[0].h, (new Date(sorted[0].t*1000)).toISOString())
          console.warn()
        }
        countableMinorVersions.push({
          minorVersion: minorVersionIdx,
          changeset: sorted[0].c,
          validSince: sorted[0].t,
          user:       sorted[0].h,
          uid:        sorted[0].u,
          coordinates:mV.map(function(p){return p.p})
        })
        minorVersionIdx++;
      }
      /* else{
        if(DEBUG){
          console.warn("SKIPPED: ")
          console.warn(mV.map(function(n){return n.h}).join(" > "))
          console.warn(sorted[0].h, (new Date(sorted[0].t*1000)).toISOString())
        }
      } */
      prevTimestamp = sorted[0].t;
    })
  }

  return {
    majorVersion: majorVersion.map(function(g){return g.p}),
    minorVersions: countableMinorVersions
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
      console.warn(`\n  Going for Major Version: ${that.versions[i].i} with timestamp ${that.versions[i].t} (${(new Date(that.versions[i].t*1000)).toISOString()})
      by ${that.versions[i].h} c:${that.versions[i].c}`)
    }

    //If it's not the first entry, there is a previous major version that covers these changes, so only worry about changes since this version came to be.
    if(i>0){
      validSince = that.versions[i].t
    }
    // //If there's another version to come, set validUntil to the next version
    if(i < that.versions.length-1){
      validUntil = that.versions[i+1].t - CHANGESET_THRESHOLD
    }

    //Now construct all possible geometries for this Major Version:
    //Breaking Case: If it's not 'visible' and version has no nodes, then don't try to create a geometry...
    if( that.versions[i].hasOwnProperty('n') ){
      //Construct all possible geometries for this version, based on the nodeRefs.

      var majorVersionNumber = that.versions[i]['i']

      var geometries = that.buildAllPossibleVersionGeometries({
        nodeRefs: that.versions[i].n,
        validSince: validSince,
        validUntil: validUntil,
        changeset: that.versions[i].c
      })

      if(geometries.majorVersion){
        that.historicalGeometries[majorVersionNumber] = [{
          type:"Feature",
          properties:{
            '@version': majorVersionNumber,
            '@minorVersion': 0,
            '@user' : that.versions[i].h,
            '@changeset' : that.versions[i].c,
            '@uid' : that.versions[i].u,
            '@validSince': that.versions[i].t,
            '@validUntil': (i<that.versions.length-1)? that.versions[i+1].t: null
          },
          geometry: {
            type:"LineString",
            coordinates : geometries.majorVersion
          }
        }]
      }

      if (geometries.minorVersions && geometries.minorVersions.length>0){
        //Iterate through the minorVersions, amending the validUntil fields...
        //Reset the validUntil of the major Version with minorVersion_1
        that.historicalGeometries[majorVersionNumber][0].properties['@validUntil'] = geometries.minorVersions[0]["validSince"]

        for(var j=0; j < geometries.minorVersions.length; j++){
          var mV = geometries.minorVersions[j];
          that.historicalGeometries[majorVersionNumber].push({
            type:"Feature",
            geometry:{
              type:"LineString",
              coordinates: mV.coordinates
            },
            properties:{
              '@version':majorVersionNumber,
              '@minorVersion':mV.minorVersion,
              '@changeset':mV.changeset,
              '@user':mV.user,
              '@uid' :mV.uid,
              '@validSince':mV.validSince,
              '@validUntil': (j<geometries.minorVersions.length-1)? geometries.minorVersions[j+1].validSince: null
            }
          })
        }

        //If there is another major version coming afer this one... reset the LAST value of minor Versions...
        if (i<that.versions.length-1){
          that.historicalGeometries[majorVersionNumber][that.historicalGeometries[majorVersionNumber].length-1].properties['@validUntil'] = that.versions[i+1].t
        }

        if(DEBUG){
          console.warn(`Major Version: ${majorVersionNumber} has ${geometries.minorVersions.length} minor versions`)
        }
      }
    }
  }
}
}
