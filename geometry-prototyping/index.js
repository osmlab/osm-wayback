/*
  Read in a stream of GeoJSON (history enriched ... and process geometries)
*/

var topojson = require("topojson");

const DEBUG=1;

var INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS = 0;
var INCLUDE_FULL_PROPERTIES_ON_MINOR_VERSIONS = 0;
var INCLUDE_MAJOR_DIFFS                       = 1;

//ONLY ONE OF THESE SHOULD BE SET
var WRITE_HISTORY_COMPLETE_OBJECT   = 1;
var WRITE_EVERY_GEOMETRY            = 1;
var WRITE_TOPOJSON_HISTORY          = 1;

var allGeometriesByteSize               = 0;
var historyCompleteSingleObjectByteSize = 0;
var topojsonHistoryByteSize             = 0;
var string;

console.error("Beginning Geometry Reconstruction")

process.stdin.pipe(require('split')())
  .on('data', processLine)
  .on('end',function(){
    process.stderr.write(`\n\nOutput Sizes (based on string length):`);
    process.stderr.write(`\n--Individual Geometries    : ${ (allGeometriesByteSize / (1024*1024)).toFixed(1)} MB`);
    process.stderr.write(`\n--History Object           : ${ (historyCompleteSingleObjectByteSize / (1024*1024)).toFixed(1)} MB`);
    process.stderr.write(`\n--History Object (topojson): ${ (topojsonHistoryByteSize / (1024*1024)).toFixed(1)} MB\n\n`);
  })

// Create a geometry builder instance for the data on this line
var GeometryBuilder = require('./geometry-builder.js')

var geometries = 0;

function processLine (line) {
  //If line is empty, skip
  if (line==="") return;

  var object = JSON.parse(line);

  //If there are nodeLocations, there is potential for multiple historical geometries (even if only 1 major Version)
  if (object.hasOwnProperty('nodeLocations')){

    if ( object.properties.hasOwnProperty('@history') ){
      var geometryBuilder = new GeometryBuilder({
        'nodeLocations' : object.nodeLocations,
        'history'       : object.properties['@history'],
        'osmID'         : object.properties['@id']
      })
    }else{
      //Object doesn't have a history value, confirm there aren't multiple versions of nodes in nodeLocations?
      flag = false;
      Object.keys(object.nodeLocations).forEach(function(nodeID){
        if (Object.keys(object.nodeLocations[nodeId]).length > 1){
          flag = true;
        }
      })
      if(flag){
        //TODO
        console.error("\n Situation with version 1, no history, but multiple nodeLocations \n")
      }
    }

    //Build possible geometries from NodeLocations
    geometryBuilder.buildGeometries();
    geometries++;

    //Construct a new, minorVersion enabled historical version of the object:
    var newHistoryObject = []

    var geometryType = object.geometry.type;

    //Iterate through the current @history, reconstructing the geometries
    if(object.properties.hasOwnProperty('@history')){

      var majorVersionTags = {};

      object.properties['@history'].forEach(function(histObj){

        //Reconstruct the base properties for this Major Version
        majorVersionTags = reconstructMajorOSMTags(majorVersionTags, histObj)

        var majorVersionKey = histObj.i;
        for(var i in geometryBuilder.historicalGeometries[majorVersionKey]){

          //Reconstruct Polygons from LineStrings, if necessary?
          if(geometryType==="Polygon" || geometryType==="MultiPolygon"){
            geometryBuilder.historicalGeometries[majorVersionKey][i].geometry.type = "Polygon"
            geometryBuilder.historicalGeometries[majorVersionKey][i].geometry.coordinates = [geometryBuilder.historicalGeometries[majorVersionKey][i].geometry.coordinates]
          }

          var minorVersion = {
            type:"Feature",
            geometry: geometryBuilder.historicalGeometries[majorVersionKey][i].geometry,
            properties: geometryBuilder.historicalGeometries[majorVersionKey][i].properties
          }



          if(i==0){
            if (INCLUDE_MAJOR_DIFFS){
              minorVersion.properties = {...minorVersion.properties, ...histObj};
              minorVersion.properties['@user']      = histObj.h;
              delete minorVersion.properties.h;
              minorVersion.properties['@uid']       = histObj.u;
              delete minorVersion.properties.u;
              minorVersion.properties['@changeset'] = histObj.c;
              delete minorVersion.properties.c;
              minorVersion.properties['@version']   = histObj.i;
              delete minorVersion.properties.i;
              delete minorVersion.properties.t;
            }
            if(INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS){
              minorVersion.properties = {...minorVersion.properties, ...majorVersionTags}
              minorVersion.properties['@id'] = object.properties['@id']
            }
          }else{
            if (INCLUDE_FULL_PROPERTIES_ON_MINOR_VERSIONS){
              minorVersion.properties = {...minorVersion.properties, ...majorVersionTags}
              minorVersion.properties['@id'] = object.properties['@id']
            }
          }

          delete minorVersion.properties.n;

          if (WRITE_EVERY_GEOMETRY){
            string = JSON.stringify(minorVersion)
            allGeometriesByteSize += string.length;
            console.log(string)
          }

          newHistoryObject.push(minorVersion)

        }
      })

      //Fix up the history of the original object?
      object.properties['@history'] = newHistoryObject;
      delete object.nodeLocations;
      delete object.properties['@way_nodes']

      if(WRITE_HISTORY_COMPLETE_OBJECT){
        string = JSON.stringify(object)
        historyCompleteSingleObjectByteSize += string.length;
        console.log(string)
      }

      //Encode TopoJSON
      if(WRITE_TOPOJSON_HISTORY){
        object.properties['@history'] = topojson.topology(newHistoryObject)
        string = JSON.stringify(object)
        console.log(string)
        topojsonHistoryByteSize += string.length;
      }
    }

    //TODO: add geometries even if there is no history?
  }

  process.stderr.write(`\r${geometries} processed`);
}

/**
 *  Helper function to reconstruct properties between major Versions from diffs
*/
function reconstructMajorOSMTags(baseObject,newObject){

  if (newObject.hasOwnProperty('aA') && newObject.aA){
    Object.keys(newObject.aA).forEach(function(key){
      baseObject[key] = newObject.aA[key]
    })
  }
  if (newObject.hasOwnProperty('aM') && newObject.aM){
    Object.keys(newObject.aM).forEach(function(key){
      baseObject[key] = newObject.aM[key][1]
    })
  }
  if (newObject.hasOwnProperty('aD') && newObject.aD){
    Object.keys(newObject.aD).forEach(function(key){
      delete baseObject[key]
    })
  }
  return baseObject
}
