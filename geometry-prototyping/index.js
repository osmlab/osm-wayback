/*

  Read in a stream of GeoJSON (history enriched ... and process geometries)

*/

const opts = {
    readOnly: true,
    create_if_missing: false,
    paranoid_checks: true,
    use_fsync: false,
    allow_mmap_reads: false,
    use_direct_reads: false,
    allow_fallocate: true,
    is_fd_close_on_exec: true,
    advise_random_on_open: true,
    new_table_reader_for_compaction_inputs: true,
    use_adaptive_mutex: false,
    enable_thread_tracking: false,
    allow_concurrent_memtable_write: true,
    enable_write_thread_adaptive_yield: true,
    skip_stats_update_on_db_open: false,
    allow_2pc: false,
    fail_if_options_file_error: false,
    dump_malloc_stats: false,
    avoid_flush_during_recovery: false,
    avoid_flush_during_shutdown: false,
    max_open_files: -1,
    max_file_opening_threads: 16,
    max_total_wal_size: 0,
    base_background_compactions: 1,
    max_background_compactions: 1,
    max_subcompactions: 1,
    max_background_flushes: 1,
    max_log_file_size: 0,
    log_file_time_to_roll: 0,
    keep_log_file_num: 1000,
    recycle_log_file_num: 0,
    table_cache_numshardbits: 0,
    WAL_ttl_seconds: 0,
    WAL_size_limit_MB: 0,
    manifest_preallocation_size: 4 * 1024 * 1024,
    db_write_buffer_size: 0,
    compaction_readahead_size: 0,
    random_access_max_buffer_size: 1024 * 1024,
    bytes_per_sync: 0,
    wal_bytes_per_sync: 0,
    write_thread_max_yield_usec: 100,
    write_thread_slow_yield_usec: 3
  }

const rocksdb = require('rocksdb-node')

const db = rocksdb.open(opts, '/data/ROCKSDB/DENBOULDER/')

const columnFamilies = db.getColumnFamilies()

console.warn("Opened RocksDB, Found: ", columnFamilies)

// process.stdin.pipe(require('split')()).on('data', processLine)

var object;

// db.get({buffer:true},columnFamilies[1], "1000470506!1",function (err, val) {
//     if (err) return console.error(err);
//     console.log("What")
//     console.log(val)
// })

// const valueBuffer = db.get({buffer:true},'nodes',"103074754!1");
// console.log(valueBuffer);

const it = db.newIterator('nodes')
it.seekToFirst()
firstKey = it.key()

console.log(firstKey)

console.log( db.get({buffer:true}, columnFamilies[1], it.key()) )

console.log(it.key())



// for (it.seekToFirst('103074754!1'); it.valid(); it.next()) {
//   const v = it.value({buffer: true}) //Parse the result?
//   console.log(it.key())
//   id = it.key().split('!')[0]
//   if (id != '1029908232'){
//     break;
//   }
// }

function processLine (line) {
  if (line==="") return;

  try{

    object = JSON.parse(line);
    if (object.properties.hasOwnProperty('@history') ){
      //We've got an object with history, time to get to work:

      var id = object.properties['@id']

      object.properties['@history'].forEach(function(histObj){

        //Node references, begin!
        if ( histObj.hasOwnProperty("n") ){
          histObj.n.forEach(function(nodeRef){
            // var lookUp = nodeRef.toString() + "!" + "1"
            // console.warn(lookUp)
            var v1 = db.get('nodes',lookUp)
            // console.warn(v1)

            // console.log(nodeRef)
            // for (it.seek(nodeRef+"1"+"1"); it.valid(); it.next()) {
              // console.log(iter.key(), iter.value())
            // }
          })
        }
      })
    }


  }catch(e){
    // console.error(e)
  }

}
