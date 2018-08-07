.com
var _ = require('lodash');

const CHANGESET_THRESHOLD            = 60*1   // 1 minute
const DEBUG = 1;

module.exports = function(osmObject){

//Attributes of given OSM element and all possible versions
this.versions      = osmObject.history;
this.osmID         = osmObject.osmID;

//A map of potential historical geometries available at this point
this.historicalGeometries = {};

/**
 *  Iterate through an OSM object's history and construct all possible geometries
 *
 *  Expects: Nothing, call on Object.
 *
 *  Returns: Nothing, populates the ``historicalGeometries`` attribute.
*/
this.buildGeometries = function(){
  var that = this;

  if(DEBUG){
    console.warn(`Processing Object: ${that.osmID}`)
  }

  //Versions is an object's history, length should correspond to current 'v'
  for(var i=0; i<that.versions.length; i++){
    validSince = false;
    validUntil = false;

    if(DEBUG){
      console.warn(`\n  Going for Major Version: ${that.versions[i].i} with timestamp ${that.versions[i].t} (${(new Date(that.versions[i].t*1000)).toISOString()})
      by ${that.versions[i].h} c:${that.versions[i].c}`)

      console.warn(`${JSON.stringify(that.versions[i].p)}`)
    }

    //If it's not the first entry, there is a previous major version that covers these changes, so only worry about changes since this version came to be.
    if(i>0){
      validSince = that.versions[i].t
    }
    // //If there's another version to come, set validUntil to the next version
    if(i < that.versions.length-1){
      validUntil = that.versions[i+1].t - CHANGESET_THRESHOLD
    }
    //just go through the 'p' attribute of nodes

  }
}
}
