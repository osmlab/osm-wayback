/*
 *  Input:  A stream of history enriched GeoJSON (GEOJSONSEQ)
 *          with embedded historical node location information
 *          See README for more
 *
 *  Output: Historical geometries written to individual versions
 *
 *
 *
*/

const DEBUG = true;

const CONFIG = {
  //Choose this...
  'GEOMETRY_ONLY'                             : false, //Only @validSince, @validUntil on ALL objects

  //OR
  'INCLUDE_DIFFS_ON_MAJOR_VERSIONS'           : true,//DIFFS don't go on minor versions
  'INCLUDE_FULL_PROPERTIES_ON_MAJOR_VERSIONS' : false,

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
