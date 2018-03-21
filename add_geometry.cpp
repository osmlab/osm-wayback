/*

  USAGE: cat <LINE-DELIMITED GEOJSON> add_history <INDEX DIR>

  Reads a stream of GeoJSON objects (line-delimited) and looks up the previous
  versions of each object in the rocksdb INDEX.

  It outputs a modified, enriched version of the object with the `@history`
  property if there is any history.

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
      std::set<int64_t> nodeRefs;
      // std::cout << "OSM ID: " << osm_id << std::endl;

      try{
        //Iterate through the history object, looking for node references
        for (auto& histObj : geojson_doc["properties"]["@history"].GetArray()){

          //If there are node references
          if (histObj.HasMember("n") ){
            //Add them to the nodeRefs set.
            for (auto& nodeRef : histObj["n"].GetArray()){
              nodeRefs.insert(nodeRef.GetInt64());
            }
          }
        }

        //Now iterate through the node references and get all of the versions of the node; still need to find best way to do that ... is it prefix matching?

        bool status;
        std::string rocksEntry;

        rapidjson::Document nodes;
        nodes.SetObject();

        rapidjson::Value nodeHistory(rapidjson::kArrayType);

        std::cout << "OSM ID: " << osm_id << std::endl;

        for (std::set<std::int64_t>::iterator it=nodeRefs.begin(); it!=nodeRefs.end(); ++it){

          //How do we know what versions to lookup? Herein lies the main problem...
          status = store->fetch_node_history(*it, &nodeHistory);

          if (status){

            // rapidjson::StringBuffer buffer;
            // rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            // nodeHistory.Accept(writer);
            // std::cout << nodeHistory.GetString() << std::endl;

            // std::string nodeKey = (*it).to_string();
            // std::string nodeKeyStr = std::to_string(*it);
            // rapidjson::Value nodeKey(nodeKeyStr.c_str(), nodes.GetAllocator());
            // nodes.AddMember(nodeKey, nodeHistory, nodes.GetAllocator());
          }

        }

        // rapidjson::StringBuffer buffer;
        // rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        // nodes.Accept(writer);
        // std::cout << buffer.GetString() << std::endl;

        std::cout << "===============" << std::endl;

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
