/*

  Read in a stream of GeoJSON (history enriched ... and process geometries)

*/

const rocksdb = require('rocksdb-node')
const db = rocksdb.open({readOnly:true}, '/data/ROCKSDB/DENBOULDER')
// const value = db.get('node')


process.stdin.pipe(require('split')()).on('data', processLine)

function processLine (line) {
  console.log(line + '!')
}
