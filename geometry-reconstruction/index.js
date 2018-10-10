'use strict';

var streamReduce = require('json-stream-reduce');
var path         = require('path');
var fs           = require('fs');

var lines = 0;

var status = {
  linesProcessed                : 0,
  noHistory                     : 0,
  jsonParsingError              : 0,
  noNodeLocations               : 0,
  geometryBuilderFailedToDefine : 0,
  totalGeometries               : 0,
  processLineFailures           : 0,
  topoJSONEncodingError         : 0,
  allGeometriesByteSize               :0,
  historyCompleteSingleObjectByteSize :0,
  topojsonHistoryByteSize             :0
}

var file;
if (process.argv.length > 2){
  var file = process.argv[2]
}

streamReduce({
  map: path.join(__dirname, 'map-geom-reconstruction.js'),
  file: (file)? file : path.join(__dirname, 'test/tampa-test.history.geometries.head100'),
  // maxWorkers:2,
})
.on('reduce', function(res) {
  if(res.lineProcessed)                 status.linesProcessed                ++
  if(res.noHistory)                     status.noHistory                     ++
  if(res.jsonParsingError)              status.jsonParsingError              ++
  if(res.geometryBuilderFailedToDefine) status.geometryBuilderFailedToDefine ++
  if(res.processLineFailures)           status.processLineFailures           ++
  if(res.topoJSONEncodingError)         status.topoJSONEncodingError         ++
  status.noNodeLocations                     += res.noNodeLocations;
  status.totalGeometries                     += res.totalGeometries;
  status.allGeometriesByteSize               += res.allGeometriesByteSize;
  status.historyCompleteSingleObjectByteSize += res.historyCompleteSingleObjectByteSize;
  status.topojsonHistoryByteSize             += res.topojsonHistoryByteSize;
})
.on('end', function() {
  console.warn(JSON.stringify(status,null,2))
});
