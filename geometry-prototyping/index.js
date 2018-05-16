/*

  Read in a stream of GeoJSON (history enriched ... and process geometries)

*/

console.error("Beginning Geometry Reconstruction")

process.stdin.pipe(require('split')()).on('data', processLine)

// Create a geometry builder instance for the data on this line
var GeometryBuilder = require('./geometry-builder.js')

var geometries = 0;

function processLine (line) {
  //If line is empty, skip
  if (line==="") return;

  object = JSON.parse(line);

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

    //TODO: add geometries even if there is no history?

    //Now that geometries are built, enrich history object with them
    // object.properties['@history'].forEach(function(histObj){

    // if (histObj.i == 1){
      //
      // if( geometryBuilder.historicalGeometries.hasOwnProperty(histObj.i) ){
      //         console.log(JSON.stringify(
      //           geometryBuilder.historicalGeometries[histObj.i])
      //         )
      //         console.log(JSON.stringify(
      //           {type:"Feature",
      //            properties: {
      //              'id':object.properties['@id'],
      //              'v':histObj.i
      //            },
      //            geometry: { type:  "LineString",
      //               coordinates: histObj.geometry
      //             }
      //           }));
      //       }
      //
      //     }
  } //end if nodeLocations

  process.stderr.write(`\r${geometries} processed`);

} //end processLine



    // if (object.properties.hasOwnProperty('@history') ){
    //   //We've got an object with history, time to get to work:
    //
    //   var id = object.properties['@id']
    //
    //   object.properties['@history'].forEach(function(histObj){
    //
    //     //Node references, begin!
    //     // if ( histObj.hasOwnProperty("n") ){
    //     //   histObj.n.forEach(function(nodeRef){
    //     //     // var lookUp = nodeRef.toString() + "!" + "1"
    //     //     // console.warn(lookUp)
    //     //     // var v1 = db.get('nodes',lookUp)
    //     //     // console.warn(v1)
    //     //
    //     //     // console.log(nodeRef)
    //     //     // for (it.seek(nodeRef+"1"+"1"); it.valid(); it.next()) {
    //     //       // console.log(iter.key(), iter.value())
    //     //     // }
    //     //   })
    //     // }
    //   })
    // }

    //Print the object back to the console.
    // console.log(JSON.stringify(object))

  // }catch(e){
      // console.error(e)
      // console.error(e.backtrace)
      // throw(e)
  // }
