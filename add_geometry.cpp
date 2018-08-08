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

void fetchNodeGeometries(ObjectStore* store, const std::string line) {
    rapidjson::Document geojson_doc;

    if(geojson_doc.Parse<0>(line.c_str()).HasParseError()) {
        std::cerr << "ERROR" << std::endl;
        return;
    }

    const std::string obj_type = geojson_doc["properties"]["@type"].GetString();

    //If object is not a node, there is a @history property with nodeRefs.
    if (obj_type != "node"){

        try{
            //Start a set of unique node IDs ever associated with any version of this object
            std::set<std::string> nodeRefs;

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

            rapidjson::Value nodeLocations(rapidjson::kObjectType);
            /* nodeLocations will become the following object.
             * {
                  nodeID : {
                    changesetID : {
                      p: [lon, lat]
                      i: <version>
                      u: <uid>
                      h: <handle>
                    },
                    changesetID : ...
                  },
                  nodeID : ...
                }
             */

            //Iterate through the set of unique node IDs associated with this object
            for (std::set<std::string>::iterator it=nodeRefs.begin(); it!=nodeRefs.end(); ++it){

                std::string rocksEntry;
                rocksdb::Status status = store->get_node_locations(*it, &rocksEntry);

                //rocksEntry is now the string from rocksDB, parse it into JSON
                if(status.ok()){
                    rapidjson::Value nodeIDStr;
                    nodeIDStr.SetString(*it, geojson_doc.GetAllocator()); //Set the ID of the node

                    rapidjson::Document thisNodeHistory;
                    thisNodeHistory.Parse<rapidjson::kParseFullPrecisionFlag>( rocksEntry.c_str() );

                    //DEBUGGING: Print out the string from rocksDB
                    // std::cerr << rocksEntry.c_str() << std::endl;

                    //A new object we'll deepcopy values into?
                    rapidjson::Value thisNodeHistoryNew(rapidjson::kObjectType);

                    //Iterate through the history of this individual node
                    for ( rapidjson::Value::ConstMemberIterator itr = thisNodeHistory.MemberBegin();
                          itr != thisNodeHistory.MemberEnd();
                          ++itr) {   //iterate through object

                        // std::cerr << itr->name.GetString() << " "; //key name

                        rapidjson::Value changesetID;
                        changesetID.SetString(itr->name.GetString(), geojson_doc.GetAllocator());

                        rapidjson::Value nodeVersion(rapidjson::kObjectType);
                        nodeVersion.SetObject();

                        rapidjson::Value handle;
                        handle.SetString(itr->value["h"].GetString(), geojson_doc.GetAllocator());
                        nodeVersion.AddMember("h",handle,geojson_doc.GetAllocator());

                        rapidjson::Value uid;
                        uid.SetInt(itr->value["u"].GetInt());
                        nodeVersion.AddMember("u",uid,geojson_doc.GetAllocator());

                        rapidjson::Value version;
                        version.SetInt(itr->value["i"].GetInt());
                        nodeVersion.AddMember("i",version,geojson_doc.GetAllocator());

                        rapidjson::Value timestamp;
                        timestamp.SetInt64(itr->value["t"].GetInt64());
                        nodeVersion.AddMember("t",timestamp,geojson_doc.GetAllocator());

                        rapidjson::Value changeset;
                        changeset.SetInt64(itr->value["c"].GetInt64());
                        nodeVersion.AddMember("c",changeset,geojson_doc.GetAllocator());

                        if(itr->value["p"].IsArray()){
                            rapidjson::Value coordinates(rapidjson::kArrayType);
                            coordinates.PushBack(itr->value["p"][0].GetDouble(), geojson_doc.GetAllocator());
                            coordinates.PushBack(itr->value["p"][1].GetDouble(), geojson_doc.GetAllocator());
                            nodeVersion.AddMember("p",coordinates,geojson_doc.GetAllocator());
                        }

                        thisNodeHistoryNew.AddMember(changesetID,nodeVersion,geojson_doc.GetAllocator());
                    }

                    nodeLocations.AddMember(nodeIDStr,thisNodeHistoryNew,geojson_doc.GetAllocator());

                }else{
                    std::cerr << "Node Lookup failed on " << *it << std::endl;
                }
            }
            if (!nodeLocations.Empty()){
                geojson_doc.AddMember("nodeLocations",nodeLocations,geojson_doc.GetAllocator());
            }else{
              //Node locations is empty
            }

        } catch (const std::exception& ex) {
            std::cerr<< ex.what() << std::endl;
        }
    }

    //Now write the object back out
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    geojson_doc.Accept(writer);
    std::string s(buffer.GetString(), buffer.GetSize());

    //Write new geojson_doc with nodeLocations to stdout
    std::cout << s << std::endl;
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
