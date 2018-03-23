/*

  Read in a stream of GeoJSON (history enriched ... and process geometries)

*/

console.error("Beginning Geometry Reconstruction")

process.stdin.pipe(require('split')()).on('data', processLine)

var GeometryBuilder = require('./geometry-builder.js')

var geometries = 0;

function processLine (line) {
  if (line==="") return;

  // try{

    object = JSON.parse(line);

    if (object.hasOwnProperty('nodeLocations')){
      if (object.properties.hasOwnProperty('@history') ){
        var geometryBuilder = new GeometryBuilder(object.nodeLocations, object.properties['@history'], object.properties['@id']);

        // console.warn(`\n\nOSM ID: ${object.properties['@id']}, hist versions: ${object.properties['@history'].length}`)
        // console.warn("==================================================")

        geometryBuilder.buildGeometries();
        geometries++;

        object.properties['@history'].forEach(function(histObj){
          // if (histObj.i == 1){

            if( geometryBuilder.majorVersions.hasOwnProperty(histObj.i) ){
              histObj.geometry = geometryBuilder.majorVersions[histObj.i]
              console.log(JSON.stringify(
                {type:"Feature",
                 properties: {
                   'id':object.properties['@id'],
                   'v':histObj.i
                 },
                 geometry: { type:  "LineString",
                    coordinates: histObj.geometry
                  }
                }));
            }

          // }

        });
      }
    }

    process.stderr.write(`\r${geometries} processed`);

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

}
