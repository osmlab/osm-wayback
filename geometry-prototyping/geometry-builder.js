var _ = require('lodash');

module.exports = function(nodeLocations, history){

  this.nodeLocations = nodeLocations;
  this.versions      = history;

  // console.log(this.nodeLocations)
  // console.log("===============")
  // function simplifyNodeHistory(nodes){
  //   Object.keys(nodes).forEach(function(nodeID){
  //     console.log(nodes[nodeID])
  //     console.log("Woah")
  //   })
  // }

  // simplifyNodeHistory(this.nodeLocations);

  //Given a node reference and possible dates, return versions of the node that _could be_.
  this.getNodeVersions = function(nodeRef, validSince, validUntil){
    var that = this;
    console.warn("Looking up node: "+nodeRef);
    nodeVersionsByChangeset = that.nodeLocations[nodeRef.toString()];

    if (nodeVersionsByChangeset==undefined){
      console.error("No version of Node");
      return null;
    }
    // Ensure node versions are sorted appropriately
    var nodeVersions = _.sortBy(Object.values(nodeVersionsByChangeset),function(n){return n.t});

    //If there's only one version of the node, use it (always)
    if (nodeVersions.length==1){ return nodeVersions; }//

    //Be prepared to fall through and return the closest node less than validSince.
    var prevNode = nodeVersions[0]; //First version of the node
    var filteredNodes = []

    if(validSince){
      //Cut out previous versions, but save t-1
      nodeVersions.forEach(function(node){
        if (node.t >= validSince){
          filteredNodes.push(node)
        }
        prevNode = node;
      })
      //Safety condition 1: If there are now NO NODES, return prevNode
      if(filteredNodes.length==0){
        return prevNode;
      }
    }else{
      filteredNodes = nodeVersions;
    }

    //If we have a validUntil, then filter the future nodes out
    if (validUntil){
      filteredNodes = filteredNodes.filter(function(v){return v.t < validUntil})
      //If this removed all nodes, then we have a bit of a problem
      if(filteredNodes.length==0){
        console.error("validUntil filtered out all nodes")
        return null;
      }
    }
    return filteredNodes;
  }

  this.buildAllPossibleGeometries = function(nodeRefs, validSince, validUntil){
    var that = this;
    console.warn(`Building all possible geometries between ${validSince} - ${validUntil}`)

    //Iterate through the nodes, get the ones that matter
    nodeRefs.forEach(function(nodeRef){
      var possibleNodes = that.getNodeVersions(nodeRef, validSince, validUntil);
      if(possibleNodes){
        //This is where we intelligently build out geometry versions.
        //If there are multiple possible nodes, then we split out geometry...ugh.
      }else{
        console.log("NO NODES RETURNED")
      }
      console.log(possibleNodes)
    })

    return []
  }

  this.buildGeometries = function(){
    var that = this;
    var validSince, validUntil;

    console.log(that.nodeLocations)

    for(var i in that.versions){
      validSince = false;
      validUntil = false;
      console.warn(`\nGoing for Major Version: ${that.versions[i].i} with timestamp ${that.versions[i].t}`)
      //If it's not the first version, there is a previous major version that covers these changes, so only worry about changes since this version came to be.
      if(i>0){
        validSince = that.versions[i].t
      }
      // //If there's another version to come, set validUntil
      if(i < that.versions.length-1){
        validUntil = that.versions[Number(i)+1].t //That's dumb, that's really dumb
      }

      //Now construct all possible geometries for this Major Version:
      var geometries = that.buildAllPossibleGeometries(that.versions[i].n, validSince, validUntil)

      console.log(`Major Version: ${that.versions[i].i} has ${geometries.length} minor versions`)

    }


      //
      // // var changeset = histVersion.c.toString();
      // // console.warn("Going for Changeset: " + changeset)
      //
      // // For the nodes in this version, construct all possible versions
      // var coordinates = [];
      // histVersion.n.forEach(function(nodeID){
      //   console.warn("nodeID: "+ nodeID)
      //   // var possibleNodes = that.nodeLocations[nodeID.toString()];
      //   console.log(possibleNodes)
      //   // if ( that.nodeLocations[nodeID.toString()].hasOwnProperty(changeset) ){
      //   //   // console.log("GOT HERE")
      //   //   // console.log(that.nodeLocations[nodeID])
      //   //   coordinates.push( that.nodeLocations[nodeID][changeset]['p'] )
      //   // }
      //   // console.log(that.nodeLocations[ nodeID ])
      // })
      // // console.log(coordinates)

  }
}
