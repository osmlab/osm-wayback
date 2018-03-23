/*

  USAGE: cat <LINE-DELIMITED GEOJSON> add_geometry <INDEX DIR>

  Reads a stream of GeoJSON objects (line-delimited) and looks up the previous
  versions of each object in the rocksdb locations INDEX.

*/

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <map>
#include <iterator>
#include <set>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"
#pragma GCC diagnostic pop

#include <osmium/geom/rapid_geojson.hpp>
#include "rocksdb/db.h"

#include "db.hpp"
#include "pbf_encoding.hpp"
#include "json_encoding.hpp"

//https://stackoverflow.com/questions/8473009/how-to-efficiently-compare-two-maps-of-strings-in-c
template <typename Map>
bool map_compare (Map const &lhs, Map const &rhs) {
    // No predicate needed because there is operator== for pairs already.
    return lhs.size() == rhs.size()
        && std::equal(lhs.begin(), lhs.end(),
                      rhs.begin());
}

int osm_type(const std::string type) {
    if (type == "node") return 1;
    if (type == "way") return 2;
    if (type == "relation") return 3;
    return 1;
}

void fetchNodeGeometries(ObjectStore* store, const std::string line) {
    rapidjson::Document geojson_doc;

    if(geojson_doc.Parse<0>(line.c_str()).HasParseError()) {
        std::cerr << "ERROR" << std::endl;
        return;
    }

    if (geojson_doc["properties"].HasMember("@history") ){

      // const auto version = geojson_doc["properties"]["@version"].GetInt();
      const auto osm_id = geojson_doc["properties"]["@id"].GetInt64();
      const std::string type = geojson_doc["properties"]["@type"].GetString();

      //A set of nodes to lookup in the index.
      std::set<std::string> nodeRefs;

      // std::cout << "OSMID: "<< osm_id << std::endl;
      // std::cout << "-----------" << std::endl;

      try{
        //Iterate through the history object, looking for node references
        for (auto& histObj : geojson_doc["properties"]["@history"].GetArray()){

          //If there are node references
          if (histObj.HasMember("n") ){
            //Add them to the nodeRefs set.
            for (auto& nodeRef : histObj["n"].GetArray()){
              nodeRefs.insert(std::to_string(nodeRef.GetInt64()));
            }
          }
        }

        std::string rocksEntry;
        rapidjson::Document thisNodeHistory;
        rapidjson::Value nodesHistory(rapidjson::kObjectType);

        for (std::set<std::string>::iterator it=nodeRefs.begin(); it!=nodeRefs.end(); ++it){

          //std::cout << "NODE: " << *it << std::endl;

          rocksdb::Status status = store->get_node_locations(*it, &rocksEntry);

          //rocksEntry is the string of node history, which is a JSON doc, add it to the array
          if(status.ok()){
              rapidjson::Value nodeIDStr;
              nodeIDStr.SetString(*it, geojson_doc.GetAllocator());
              thisNodeHistory.Parse<0>(rocksEntry.c_str());
              nodesHistory.AddMember(nodeIDStr,thisNodeHistory,geojson_doc.GetAllocator());
          }
        }

        geojson_doc.AddMember("nodeLocations",nodesHistory,geojson_doc.GetAllocator());

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        geojson_doc.Accept(writer);
        std::cout << buffer.GetString() << std::endl;

        // std::cout << "===============" << std::endl;

      } catch (const std::exception& ex) {
          std::cerr<< ex.what() << std::endl;
      }

    }
}

//https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::iscntrl(ch);
    }));

}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " INDEX_DIR" << std::endl;
        std::exit(1);
    }

    int feature_count = 0;

    std::string index_dir = argv[1];

    //TODO: Read the file in chunks, parallelize the activity
    //  - This requires opening multiple ObjectStores as follows: (readonly)
    ObjectStore store(index_dir, false);

    for (std::string line; std::getline(std::cin, line);) {
        ltrim(line);
        fetchNodeGeometries(&store, line);
        feature_count++;
        if(feature_count%100==0){
          std::cerr << "\rProcessed: " << (feature_count/1000) << " K features";
        }
    }

    if(feature_count == 0) {
        std::cerr << "No features processed" << std::endl;
        std::exit(5);
    }

}
