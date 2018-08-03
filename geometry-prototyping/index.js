/*
 *  Input:  A stream of history enriched GeoJSON (GEOJSONSEQ)
 *          with embedded historical node location information
 *
 *  Output: Historical geometries written to individual versions
*/

var topojson = require("topojson");

var WayGeometryBuilder  = require('./way-geometry-builder.js')
var NodeGeometryBuilder = require('./node-geometry-builder.js')

const DEBUG = true;

const config = {
  'INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS' : true,
  'INCLUDE_FULL_PROPERTIES_ON_MINOR_VERSIONS' : false,
  'INCLUDE_MAJOR_DIFFS'                       : true,
  'GEOMETRY_ONLY'                             : false,

  //ONLY ONE OF THESE SHOULD BE SET
  'WRITE_HISTORY_COMPLETE_OBJECT'             : false,
  'WRITE_EVERY_GEOMETRY'                      : false,
  'WRITE_TOPOJSON_HISTORY'                    : true
}

//Stats
var allGeometriesByteSize               = 0;
var historyCompleteSingleObjectByteSize = 0;
var topojsonHistoryByteSize             = 0;
var string;
var geometries = 0;

/*
 * Main Function
*/
function processLine (line) {
  //If line is empty, skip
  if (line==="") return;

  var object = JSON.parse(line);

  var geometryBuilder;

  //If the object already has `@history` data
  if (object.properties.hasOwnProperty('@history')){

    //If it's a node, initialize a simpler geometry builder
    if (object.properties['@type']==='node'){

      geometryBuilder = new NodeGeometryBuilder({
        'history' : object.properties['@history'],
        'osmID'   : object.properties['@id']
      })

    }else if (object.hasOwnProperty('nodeLocations')) {
      geometryBuilder = new WayGeometryBuilder({
        'nodeLocations' : object.nodeLocations,
        'history'       : object.properties['@history'],
        'osmID'         : object.properties['@id']
      })
    }

    if (geometryBuilder){
      geometryBuilder.buildGeometries();
      geometries++;

      //Build the output object
      var geometryType = object.geometry.type;

      var newHistoryObject = []
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

          if(config.GEOMETRY_ONLY){
            minorVersion.properties = {
              '@validSince':geometryBuilder.historicalGeometries[majorVersionKey][i].properties['@validSince'],
              '@validUntil':geometryBuilder.historicalGeometries[majorVersionKey][i].properties['@validUntil']
            }
          }

          if(i==0){
            if (config.INCLUDE_MAJOR_DIFFS){
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
            if(config.INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS){
              minorVersion.properties = {...minorVersion.properties, ...majorVersionTags}
              minorVersion.properties['@id'] = object.properties['@id']
            }
          }else{
            if (config.INCLUDE_FULL_PROPERTIES_ON_MINOR_VERSIONS){
              minorVersion.properties = {...minorVersion.properties, ...majorVersionTags}
              minorVersion.properties['@id'] = object.properties['@id']
            }
          }

          delete minorVersion.properties.n;

          if (config.WRITE_EVERY_GEOMETRY){
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

      if(config.GEOMETRY_ONLY){
        object.properties = {
          '@validSince' : object.properties['@timestamp'],
          '@history'    : object.properties['@history']
        }
      }

      if(config.WRITE_HISTORY_COMPLETE_OBJECT){
        string = JSON.stringify(object)
        historyCompleteSingleObjectByteSize += string.length;
        console.log(string)
      }

      //Encode TopoJSON
      if(config.WRITE_TOPOJSON_HISTORY){
        try{
          object.properties['@history'] = topojson.topology(newHistoryObject)
          string = JSON.stringify(object)
          console.log(string)
          topojsonHistoryByteSize += string.length;
        }catch(err){
          console.error("err")
        }
      }
    }

    //TODO: add geometries even if there is no history?
  }else{
    //If there is no history, let's just write it back out...
    console.log(line)
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

/*
 * RUNTIME
*/

console.error("Beginning Geometry Reconstruction")

process.stdin.pipe(require('split')())
  .on('data', processLine)
  .on('end',function(){
    process.stderr.write(`\n\nOutput Sizes (based on string length):`);
    process.stderr.write(`\n--Individual Geometries    : ${ (allGeometriesByteSize / (1024*1024)).toFixed(1)} MB`);
    process.stderr.write(`\n--History Object           : ${ (historyCompleteSingleObjectByteSize / (1024*1024)).toFixed(1)} MB`);
    process.stderr.write(`\n--History Object (topojson): ${ (topojsonHistoryByteSize / (1024*1024)).toFixed(1)} MB\n\n`);
  })
