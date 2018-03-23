var _ = require('lodash');

const MINOR_VERSION_SECOND_THRESHOLD = 60*15; //15 minutes

module.exports = function(nodeLocations, history, osmID){

  this.nodeLocations = nodeLocations;
  this.versions      = history;
  this.osmID         = osmID;

  this.majorVersions = {};

  //Given a node reference and possible dates, return versions of the node that _could be_.
  this.getNodeVersions = function(nodeRef, validSince, validUntil, changeset){
    var that = this;
    // console.warn("Looking up node: "+nodeRef);
    nodeVersionsByChangeset = that.nodeLocations[nodeRef.toString()];

    if (nodeVersionsByChangeset==undefined){
      console.error("No version of Node");
      return null;
    }
    // Ensure node versions are sorted appropriately
    var nodeVersions = _.sortBy(Object.values(nodeVersionsByChangeset),function(n){return n.t});

    //Filter to only have geometries... otherwise less interested
    nodeVersions = nodeVersions.filter(function(n){return n.hasOwnProperty('p')})

    //If there's only one version of the node, use it (always)
    if (nodeVersions.length==1){ return nodeVersions; }//

    //Be prepared to fall through and return the closest node less than validSince.
    var prevNode = nodeVersions[0]; //First version of the node
    var filteredNodes = []

    if(validSince){
      //Cut out previous versions, but save t-1
      nodeVersions.forEach(function(node){
        //
        // OVERRIDE 1: Always add if changeset matches (the timestamp will be less, but changeset will match)
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

    //stay safe from atomic changes below...
    var filterable = JSON.parse(JSON.stringify(filteredNodes));

    //If we have a validUntil, then filter the future nodes out
    if (validUntil){
      //OVERRIDE 2: IF there is a matching changeset, be sure it doesn't get abandoned
      //create a new object
      // console.log(filterable)
      filterable = filterable.filter(function(v){return (v.t < validUntil || v.c==changeset)})

      if(filterable.length==0){
        //If this removed all nodes, then return the most recent version; there is likely a _deleted_ version later
        // console.error("validUntil filtered out all nodes")
        return [prevNode]
      }
    }
    if (filterable.length==1){
      return filterable;
    }else{
      //OVER RIDE 2: If there is not an actual geometry change, then don't return it!
      try{
        var prev = filterable[0].p
        var diffGeoms = [filteredNodes[0]];
        for(var i=1;i<filterable.length;i++){
          // if (filterable[i].hasOwnProperty('p')){
          if (prev[0]!=filterable[i].p[0] || prev[1]!=filterable[i].p[1]){
            diffGeoms.push(filterable[i])
            prev = filterable[i].p
          }
          // }
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

    throw "No Possible Nodes"
    return false
  }

  this.buildAllPossibleGeometries = function(nodeRefs, validSince, validUntil, majorVersionChangeset){
    var that = this;

    // console.warn(`Building all possible geometries between ${validSince} - ${validUntil}`)

    var majorVersion = [];
    var minorVersions = [];

    //Iterate through the nodes, get the ones that matter
    nodeRefs.forEach(function(nodeRef){
      var possibleNodes = that.getNodeVersions(nodeRef, validSince, validUntil, majorVersionChangeset);

      if( possibleNodes ){
        // console.log("YES")
        majorVersion.push(Object.assign({},possibleNodes[0]));

        // aPossibleGeometry.push(Object.assign{},possibleNodes[0]);

        //If there are still nodes, then we have a new minorVersion
        if (possibleNodes.length>1){
          // aPossibleGeometry = JSON.parse(JSON.stringify(majorVersion));
          // aPossibleGeometry.pop()//remove that node, add the fancy new one
          // aPossibleGeometry.push(possibleNodes[1])
          //There are more possibilities!
          // console.log('minor version possible')
          minorVersions.push("minorVs")
        }
      }else{
        throw "NO NODES RETURNED by getNodeVersions() for "+nodeRef
      }
      // console.log(possibleNodes)
    })

    // console.warn("MAJOR VERSION: ",majorVersion)
    // console.warn("minorVersions: ",minorVersions)

    return {
      majorVersion: majorVersion.map(function(g){return g.p})
    }
  }

  this.buildGeometries = function(){
    var that = this;
    var validSince, validUntil;

    // console.log(that.nodeLocations)

    for(var i in that.versions){
      validSince = false;
      validUntil = false;
      // console.warn(`\nGoing for Major Version: ${that.versions[i].i} with timestamp ${that.versions[i].t} by ${that.versions[i].h} c:${that.versions[i].c}`)
      //If it's not the first version, there is a previous major version that covers these changes, so only worry about changes since this version came to be.
      if(i>0){
        validSince = that.versions[i].t
      }
      // //If there's another version to come, set validUntil
      if(i < that.versions.length-1){
        validUntil = that.versions[Number(i)+1].t //That's dumb, that's really dumb
      }

      //Now construct all possible geometries for this Major Version:
      //Breaking Case: If it's not 'visible' and version has no nodes, then don't try to create a geometry...
      if( that.versions[i].hasOwnProperty('n') ){
        var geometries = that.buildAllPossibleGeometries(that.versions[i].n, validSince, validUntil,that.versions[i].c)

        if(geometries.majorVersion){
          that.majorVersions[i] = geometries.majorVersion
        }
      }

      // console.log(`Major Version: ${that.versions[i].i} has ${geometries.length} minor versions`)

    }
  }
}
