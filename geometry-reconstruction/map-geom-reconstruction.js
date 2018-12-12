'use strict';

var topojson = require("topojson");
var WayGeometryBuilder      = require('./element-reconstruction/way-history-builder.js')
var NodeGeometryBuilder     = require('./element-reconstruction/node-history-builder.js')
var RelationGeometryBuilder = require('./element-reconstruction/relation-history-builder.js')

/*
*  Helper function to reconstruct properties between major Versions from diffs
*/
function reconstructMajorOSMTags(baseObject, newObject){
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

const CONFIG = {
  //Choose this...
  'GEOMETRY_ONLY'                             : false, //Only @validSince, @validUntil on ALL objects

  //OR
  'INCLUDE_DIFFS_ON_MAJOR_VERSIONS'           : false,//DIFFS don't go on minor versions
  'INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS' : true,

  //Optional
  'INCLUDE_FULL_PROPERTIES_ON_MINOR_VERSIONS' : true, //Need this if styling by attributes (such as building)

  //ONLY ONE OF THESE SHOULD BE SET...
  'WRITE_HISTORY_COMPLETE_OBJECT'             : false,
  'WRITE_EVERY_GEOMETRY'                      : true,
  'WRITE_TOPOJSON_HISTORY'                    : false
}

module.exports = function(line, writeData, done) {
  var status = {
    lineProcessed                 : false,
    noHistory                     : false,
    jsonParsingError              : false,
    noNodeLocations               : 0,
    geometryBuilderFailedToDefine : false,
    totalGeometries               : 0,
    processLineFailures           : false,
    topoJSONEncodingError         : false,
    allGeometriesByteSize               :0,
    historyCompleteSingleObjectByteSize :0,
    topojsonHistoryByteSize             :0
  }

  var geometryBuilder;
  var string

  try{
    var object = JSON.parse(line.toString());

    // console.warn(object.properties['@id'], object.properties['@version']+"\n")

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

      // This will be the case for turn restrictions(?)
      }else if (object.properties['@type']==='relation'){
        geometryBuilder = new RelationGeometryBuilder({
          'history'       : object.properties['@history'],
          'osmID'         : object.properties['@id'],
          'geometry'      : object.geometry
        })
      }else{
        status.noNodeLocations++;
      }

      //if Geometry Builder was defined, keep going!
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

          //Iterae through the historical geometries from this major version
          for(var i in geometryBuilder.historicalGeometries[majorVersionKey]){
            //i is the minor version, for nodes, it will always be 0

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
            }

            if(CONFIG.GEOMETRY_ONLY){
              thisVersion.properties = {
                '@validSince':geometryBuilder.historicalGeometries[majorVersionKey][i].properties['@validSince'],
                '@validUntil':geometryBuilder.historicalGeometries[majorVersionKey][i].properties['@validUntil']
              }
            }else{
              //Set the properties from geometryBuilder
              thisVersion.properties = geometryBuilder.historicalGeometries[majorVersionKey][i].properties; //This is the shorthand form, FYI

              thisVersion.properties['@id'] = object.properties['@id'] //Put the IDs back on individual versions

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

                if (CONFIG.INCLUDE_DIFFS_ON_MAJOR_VERSIONS){
                  if( histObj.hasOwnProperty('aA')){
                    thisVersion.properties['aA'] = histObj.aA;
                  }
                  if( histObj.hasOwnProperty('aM')){
                    thisVersion.properties['aM'] = histObj.aM;
                  }
                  if( histObj.hasOwnProperty('aD')){
                    thisVersion.properties['aD'] = histObj.aD;
                  }
                }

                // if( histObj.hasOwnProperty('aA')){
                //   if (CONFIG.INCLUDE_DIFFS_ON_MAJOR_VERSIONS){
                //     thisVersion.properties['aA'] = histObj.aA;
                //   }
                //   delete histObj.aA;
                // }
                // if( histObj.hasOwnProperty('aM')){
                //   if (CONFIG.INCLUDE_DIFFS_ON_MAJOR_VERSIONS){
                //     thisVersion.properties['aM'] = histObj.aM;
                //   }
                //   delete histObj.aM;
                // }
                // if( histObj.hasOwnProperty('aD')){
                //   if (CONFIG.INCLUDE_DIFFS_ON_MAJOR_VERSIONS){
                //     thisVersion.properties['aD'] = histObj.aD;
                //   }
                //   delete histObj.aD;
                // }

                if(CONFIG.INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS){
                  thisVersion.properties = {...thisVersion.properties, ...majorVersionTags}
                }

              }else{
                //We're in minor versions now:
                if (CONFIG.INCLUDE_FULL_PROPERTIES_ON_MINOR_VERSIONS){
                  thisVersion.properties = {...thisVersion.properties, ...majorVersionTags}
                }
              }
            }

            if ( thisVersion.hasOwnProperty('n') ){
              delete thisVersion.properties.n;
            }

            if (CONFIG.WRITE_EVERY_GEOMETRY){
              string = JSON.stringify(thisVersion)
              status.allGeometriesByteSize += string.length;
              writeData(string+"\n")
            }

            newHistoryObject.push(thisVersion)

          }
        })//End @history.forEach();

        status.totalGeometries += newHistoryObject.length;

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
          string = JSON.stringify(newHistoryObject);
          object.properties['@histrory'] = string;
          status.historyCompleteSingleObjectByteSize += string.length;
          writeData(string+"\n")
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
            writeData(string+"\n")
            status.topojsonHistoryByteSize += string.length;
          }catch(e){
            status.topoJSONEncodingError = true;
          }
        }
      }else{
        status.geometryBuilderFailedToDefine = true;
      }

      //TODO: add geometries even if there is no history?
    }else{
      //There was no history? log it and write the object back out.
      status.noHistory = true;
      writeData(line+"\n")
    }
    //If we got here, it all ran :)
    // return true;
    status.lineProcessed = true
  }catch(err){
    status.jsonParsingError = true
    console.warn(err)
  }

  done(null, status);
}
