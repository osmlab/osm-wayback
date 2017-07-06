#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"
#pragma GCC diagnostic pop

#include <osmium/geom/rapid_geojson.hpp>
#include "rocksdb/db.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " INDEX_DIR" << std::endl;
        std::exit(1);
    }

    std::string index_dir = argv[1];

    rocksdb::DB* db;
    rocksdb::Options options;
    rocksdb::Status status = rocksdb::DB::Open(options, index_dir, &db);

    rapidjson::Document doc;
    for (std::string line; std::getline(std::cin, line);) {
        if(doc.Parse<0>(line.c_str()).HasParseError()) {
            std::cout << "ERROR" << std::endl;
        } else {
            if(!doc.HasMember("properties")) continue;
            if(!doc["properties"]["@id"].IsInt() || !doc["properties"]["@version"].IsInt() || !doc["properties"]["@type"].IsString()) continue;

            const auto version = doc["properties"]["@version"].GetInt();
            const auto osm_id = doc["properties"]["@id"].GetInt();
            const std::string type = doc["properties"]["@type"].GetString();

            try {
                rapidjson::Value object_history(rapidjson::kArrayType);
                rapidjson::Document stored_doc;

                for(int v = 1; v < version; v++) {
                    const auto lookup = type + "!" + std::to_string(osm_id) + "!" + std::to_string(v);
                    std::string json;
                    rocksdb::Status s= db->Get(rocksdb::ReadOptions(), lookup, &json);
                    if (s.ok()) {
                        if(stored_doc.Parse<0>(json.c_str()).HasParseError()) {
                          continue;
                        }
                        
                        object_history.PushBack(stored_doc, doc.GetAllocator());
                    } else {
                        continue;
                    }
                }
                
                //Calculate diffs
                for(rapidjson::SizeType idx=0; idx<object_history.Size(); idx++){
                    
                    rapidjson::Value& hist_obj = object_history[idx];
                    
                    if (idx==0){
                        rapidjson::Value new_tags(rapidjson::kObjectType);

                        for(rapidjson::SizeType jdx=0; jdx<object_history[idx]["tag_keys"].Size(); jdx++){
                            new_tags.AddMember(hist_obj["tag_keys"][jdx], hist_obj["tag_values"][jdx], doc.GetAllocator());
                        }
                        hist_obj.AddMember("new_tags", new_tags, doc.GetAllocator());
                    }
                    else{
                        //For the keys in THIS version, compare to previous version
                        for(rapidjson::SizeType jdx=0; jdx<hist_obj["tag_keys"].Size(); jdx++){
                            //Search for this key in the previous version.
                            //object_history[idx-1]["tag_keys"]//.HasMember(object_history[idx]["tag_keys"][jdx]);
                            
                            //What is the result of (find)?
                            //std::cout << wtf << std::endl;
                        
                        }
                        
                        //Compare keys to the previous entry   
                    }
                }

                doc["properties"].AddMember("@history", object_history, doc.GetAllocator());


                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);
                std::cout << buffer.GetString() << std::endl;
            } catch (const std::exception& ex) {
                std::cerr<< ex.what() << std::endl;
                continue;
            }

        }
    }
}
