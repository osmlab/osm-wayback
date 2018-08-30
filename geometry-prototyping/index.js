/*
 *  Input:  A stream of history enriched GeoJSON (GEOJSONSEQ)
 *          with embedded historical node location information
 *
 *  Output: Historical geometries written to individual versions
*/

var topojson = require("topojson");

var WayGeometryBuilder  = require('./way-history-builder.js')
var NodeGeometryBuilder = require('./node-history-builder.js')
var RelationGeometryBuilder = require('./relation-history-builder.js')

const DEBUG = true;

const CONFIG = {
  //Choose this...
  'GEOMETRY_ONLY'                             : false, //Only @validSince, @validUntil on ALL objects

  //OR
  'INCLUDE_DIFFS_ON_MAJOR_VERSIONS'           : false,//DIFFS don't go on minor versions
  'INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS' : true,

  //Optional
  'INCLUDE_FULL_PROPERTIES_ON_MINOR_VERSIONS' : false,

  //ONLY ONE OF THESE SHOULD BE SET...
  'WRITE_HISTORY_COMPLETE_OBJECT'             : false,
  'WRITE_EVERY_GEOMETRY'                      : false,
  'WRITE_TOPOJSON_HISTORY'                    : true
}

//Stats
var allGeometriesByteSize               = 0;
var historyCompleteSingleObjectByteSize = 0;
var topojsonHistoryByteSize             = 0;
var string;

//Logging & debug
var linesProcessed                = 0;
var noHistory                     = 0;
var jsonParsingError              = 0;
var noNodeLocations               = 0;
var geometryBuilderFailedToDefine = 0;
var totalGeometries               = 0;
var emptyLines                    = 0;
var processLineFailures           = 0;
var topoJSONEncodingError         = 0;

/*
 * Main Function
*/
function processLine (line) {
  //If line is empty, skip
  if (line===""){
    emptyLines++;
    return false;
  }

  try{
    var object = JSON.parse(line);
  }catch(e){
    jsonParsingError++;
    return False
  }

  var geometryBuilder;

  // All objects should have a `@history` property when they get to this stage
  if (object.properties.hasOwnProperty('@history')){

    //If it's a node, initialize a simpler geometry builder
    if (object.properties['@type']==='node'){

      geometryBuilder = new NodeGeometryBuilder({
        'history' : object.properties['@history'],
        'osmID'   : object.properties['@id']
      }, CONFIG)

    //If it's not a node, then it should have a nodeLocations
    }else if (object.hasOwnProperty('nodeLocations')) {
      geometryBuilder = new WayGeometryBuilder({
        'nodeLocations' : object.nodeLocations,
        'history'       : object.properties['@history'],
        'osmID'         : object.properties['@id']
      }, CONFIG)
    }else if (object.properties['@type']==='relation'){
      geometryBuilder = new RelationGeometryBuilder({
        'history'       : object.properties['@history'],
        'osmID'         : object.properties['@id'],
        'geometry'      : object.geometry
      })
    }else{
      noNodeLocations++;
      return false
    }

    if (geometryBuilder){
      /* Populates geometryBuilder.historicalGeometries object:
        historicalGeometries = {
          <major Version1> : [minorVersion0, minorVersion1, minorVersion1, .. ],
          <major Version2  : [minorVersion0, ... ],
          ..
        }
      */
      geometryBuilder.buildGeometries();

      //Begin building output object
      var geometryType = object.geometry.type;

      //Workout any minor versions
      var newHistoryObject = []
      var majorVersionTags = {};

      object.properties['@history'].forEach(function(histObj){

        //Reconstruct the base properties for this Major Version
        majorVersionTags = reconstructMajorOSMTags(majorVersionTags, histObj)

        var majorVersionKey = histObj.i;
        for(var i in geometryBuilder.historicalGeometries[majorVersionKey]){

          //For nodes, i will always == 0

          //TODO: Is this where this belongs?
          //Reconstruct Polygons from LineStrings for ways
          if (object.properties['@type']=='way'){
            if(geometryType==="Polygon" || geometryType==="MultiPolygon"){
              geometryBuilder.historicalGeometries[majorVersionKey][i].geometry.type = "Polygon"
              geometryBuilder.historicalGeometries[majorVersionKey][i].geometry.coordinates = [geometryBuilder.historicalGeometries[majorVersionKey][i].geometry.coordinates]
            }
          }

          var thisVersion = { //Could be minor or major
            type:"Feature",
            geometry:   geometryBuilder.historicalGeometries[majorVersionKey][i].geometry,
            properties: geometryBuilder.historicalGeometries[majorVersionKey][i].properties
          }

          if(CONFIG.GEOMETRY_ONLY){
            thisVersion.properties = {
              '@validSince':geometryBuilder.historicalGeometries[majorVersionKey][i].properties['@validSince'],
              '@validUntil':geometryBuilder.historicalGeometries[majorVersionKey][i].properties['@validUntil']
            }
          }else{
            //Set basic properties from historical version (Could be minor version...)
            thisVersion.properties['@user']      = thisVersion.properties['@user']      ||  geometryBuilder.historicalGeometries[majorVersionKey][i].h;
            delete geometryBuilder.historicalGeometries[majorVersionKey][i].h;

            thisVersion.properties['@uid']       = thisVersion.properties['@uid']       || geometryBuilder.historicalGeometries[majorVersionKey][i].u;

            delete geometryBuilder.historicalGeometries[majorVersionKey][i].u;
            thisVersion.properties['@changeset'] = thisVersion.properties['@changeset'] || geometryBuilder.historicalGeometries[majorVersionKey][i].c;

            delete geometryBuilder.historicalGeometries[majorVersionKey][i].c;
            thisVersion.properties['@version']   = thisVersion.properties['@version']   || majorVersionKey
            delete geometryBuilder.historicalGeometries[majorVersionKey][i].i;

            //DIFFS ONLY BELONG ON MAJOR VERSIONS
            if(i==0){

              if( histObj.hasOwnProperty('aA')){
                if (CONFIG.INCLUDE_DIFFS_ON_MAJOR_VERSIONS){
                  thisVersion.properties['aA'] = histObj.aA;
                }
                delete histObj.aA;
              }
              if( histObj.hasOwnProperty('aM')){
                if (CONFIG.INCLUDE_DIFFS_ON_MAJOR_VERSIONS){
                  thisVersion.properties['aM'] = histObj.aM;
                }
                delete histObj.aM;
              }
              if( histObj.hasOwnProperty('aD')){
                if (CONFIG.INCLUDE_DIFFS_ON_MAJOR_VERSIONS){
                  thisVersion.properties['aD'] = histObj.aD;
                }
                delete histObj.aD;
              }

              if(CONFIG.INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS){
                thisVersion.properties = {...thisVersion.properties, ...majorVersionTags}
                thisVersion.properties['@id'] = object.properties['@id'] //set the ID
              }

            }else{
              //We're in minor versions now:
              if (CONFIG.INCLUDE_FULL_PROPERTIES_ON_MINOR_VERSIONS){
                thisVersion.properties = {...thisVersion.properties, ...majorVersionTags}
                thisVersion.properties['@id'] = object.properties['@id']
              }
            }
          }

          if ( thisVersion.hasOwnProperty('n') ){
            delete thisVersion.properties.n;
          }

          if (CONFIG.WRITE_EVERY_GEOMETRY){
            string = JSON.stringify(thisVersion)
            allGeometriesByteSize += string.length;
            console.log(string)
          }

          newHistoryObject.push(thisVersion)

        }
      })//End @history.forEach();

      totalGeometries += newHistoryObject.length;

      //Fix up the history of the original object?
      object.properties['@history'] = newHistoryObject;

      if (object.hasOwnProperty('nodeLocations') ){
        delete object.nodeLocations;
      }

      if (object.properties.hasOwnProperty('@way_nodes') ){
        delete object.properties['@way_nodes']
      }

      //Strip properties of base object as well?
      if(CONFIG.GEOMETRY_ONLY){
        object.properties = {
          '@validSince' : object.properties['@timestamp'],
          '@validUntil' : false,
          '@history'    : object.properties['@history']
        }
      }

      if(CONFIG.WRITE_HISTORY_COMPLETE_OBJECT){
        string = JSON.stringify(object)
        historyCompleteSingleObjectByteSize += string.length;
        console.log(string)
      }

      //Encode TopoJSON
      if(CONFIG.WRITE_TOPOJSON_HISTORY){
        try{
          object.properties['@history'] = JSON.stringify(topojson.topology(newHistoryObject))

          //Come back to this: If there is support for byte arrays, we can use.
          // console.warn(JSON.stringify(object, null, 2))
          // var buffer = Buffer.from(object.properties['@history'], 'utf-8')
          // var byteArray = JSON.stringify(buffer.toJSON().data)
          // console.log(byteArray);
          // var hopeItWorked = Buffer.from(JSON.parse(byteArray)).toString('utf-8')
          // console.log(hopeItWorked)

          string = JSON.stringify(object)
          console.log(string)
          topojsonHistoryByteSize += string.length;
        }catch(e){
          topoJSONEncodingError++;
          return false
        }
      }
    }else{
      geometryBuilderFailedToDefine++;
    }

    //TODO: add geometries even if there is no history?
  }else{
    //There was no history? log it and write the object back out.
    noHistory++;
    console.log(line)
  }
  //If we got here, it all ran :)
  return true;
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

//TODO: Parallelize

process.stdin.pipe(require('split')())
  .on('data', function(x){
    if (processLine(x)){
      linesProcessed++;
    }else{
      processLineFailures++;
    }
    if (linesProcessed%1000==0){
      process.stderr.write(`\r${linesProcessed} lines processed`);
    }
  })
  .on('end',function(){
    process.stderr.write(`\n\n================== REPORT ==================`);
    process.stderr.write(`\n-- Objects Processed:               ${linesProcessed}`);
    process.stderr.write(`\n-- Total Geometries Processsed:     ${totalGeometries}`);
    process.stderr.write(`\n\n------------------ ERRORS ------------------`);
    process.stderr.write(`\n-- Objects without @history attribute: ${noHistory}`);
    process.stderr.write(`\n-- TopoJSON encoding errors:           ${topoJSONEncodingError}`);
    process.stderr.write(`\n-- Missing nodeLocations object:       ${noNodeLocations}`);
    process.stderr.write(`\n-- JSON parsing errors:                ${jsonParsingError}`);
    process.stderr.write(`\n-- Failed to define geometryBuilder:   ${geometryBuilderFailedToDefine}`);
    process.stderr.write(`\n-- Empty lines in input:               ${emptyLines}`);
    process.stderr.write(`\n-- total processLine() concerns:       ${processLineFailures}`);

    process.stderr.write(`\n\nOutput Sizes (based on string length):`);
    process.stderr.write(`\n--Individual Geometries    : ${ (allGeometriesByteSize / (1024*1024)).toFixed(1)} MB`);
    process.stderr.write(`\n--History Object           : ${ (historyCompleteSingleObjectByteSize / (1024*1024)).toFixed(1)} MB`);
    process.stderr.write(`\n--History Object (topojson): ${ (topojsonHistoryByteSize / (1024*1024)).toFixed(1)} MB\n\n`);
  })
