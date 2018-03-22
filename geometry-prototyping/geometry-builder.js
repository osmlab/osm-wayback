var _ = require('lodash');

module.exports = function(nodeLocations, history){

  this.nodeLocations = nodeLocations;
  this.versions      = history;

  console.log(this.nodeLocations)
  console.log("===============")
  function simplifyNodeHistory(nodes){
    Object.keys(nodes).forEach(function(nodeID){
      console.log(nodes[nodeID])
      console.log("Woah")
    })
  }

  simplifyNodeHistory(this.nodeLocations);

  this.buildVersionGeometry = function(){
    var that = this;
    that.versions.forEach(function(histVersion){

      var changeset = histVersion.c.toString();
      console.warn("Going for Changeset: " + changeset)

      // For the nodes in this version, construct all possible versions
      var coordinates = [];
      histVersion.n.forEach(function(nodeID){
        console.warn("nodeID: "+ nodeID)
        var possibleNodes = that.nodeLocations[nodeID.toString()];
        console.log(possibleNodes)
        // if ( that.nodeLocations[nodeID.toString()].hasOwnProperty(changeset) ){
        //   // console.log("GOT HERE")
        //   // console.log(that.nodeLocations[nodeID])
        //   coordinates.push( that.nodeLocations[nodeID][changeset]['p'] )
        // }
        // console.log(that.nodeLocations[ nodeID ])
      })
      // console.log(coordinates)


    })
  }
}
