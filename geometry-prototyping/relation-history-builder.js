var _ = require('lodash');

// THIS IS A GEOMETRY FAKER...

const CHANGESET_THRESHOLD            = 60 * 5   // 5 minutes
const DEBUG = true;

var validSince
var validUntil
var thisMajorVersion

module.exports = function(osmObject, CONFIG){

  //Attributes of given OSM element and all possible versions
  this.versions        = osmObject.history;
  this.osmID           = osmObject.osmID;
  this.currentGeometry = osmObject.geometry

  //Create a map of historical geometries, keyed by version
  this.historicalGeometries = {};

  /**
   *  Iterate through an OSM object's history and assign the same geometry...
   *
   *  Expects: Nothing, call on Object.
   *
   *  Returns: Nothing, populates the `historicalGeometries` attribute.
  */
  this.buildGeometries = function(){
    var that = this;

    if(DEBUG){
      console.warn(`Processing Object: ${that.osmID}`)
    }

    //Versions is an object's history, length should correspond to current 'v'
    for(var i=0; i<that.versions.length; i++){

      thisMajorVersion = that.versions[i].i;

      if(DEBUG){
        console.warn(`\n  Going for Major Version: ${that.versions[i].i} with timestamp ${that.versions[i].t} (${(new Date(that.versions[i].t*1000)).toISOString()})
        by ${that.versions[i].h} c:${that.versions[i].c}`)
        console.warn(`${JSON.stringify(that.versions[i].p)}`)
      }

      validSince = that.versions[i].t;
      validUntil = false;

      // //If there's another version to come, set validUntil to the next version
      if(i < that.versions.length-1){
        validUntil = that.versions[i+1].t
      }

      var thisVersion = {
        geometry : that.currentGeometry,
        properties: {
          '@validSince': validSince,
          '@validUntil': validUntil
        }
      }

      thisVersion = {...thisVersion, ...that.versions[i]} //put the major properties back on

      //What other possible attributes could be included?
      that.historicalGeometries[thisMajorVersion] = [thisVersion]
    }
  }
}
