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

                for(int v = 1; v <= version; v++) {
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
                        hist_obj.AddMember("@new_tags", hist_obj["@tags"], doc.GetAllocator());
                    }
                    else{
                        const rapidjson::Value& tags = hist_obj["@tags"];
                        
                        rapidjson::Value new_tags(rapidjson::kObjectType);
                        rapidjson::Value modified_tags(rapidjson::kObjectType);
                        rapidjson::Value deleted_tags(rapidjson::kObjectType);
                        
                        for (rapidjson::Value::ConstMemberIterator it= tags.MemberBegin(); it != tags.MemberEnd(); it++){

                            rapidjson::Value tag_key(rapidjson::StringRef(it->name.GetString()));
                            rapidjson::Value tag_val(rapidjson::StringRef(it->value.GetString()));
                            
                            if (object_history[idx-1]["@tags"].HasMember(tag_key)==true) {
                            //This key exists in the previous tag list                            
                                
                                //Check if the values are the same
                                if (object_history[idx-1]["@tags"][tag_key] == tag_val ){
                                    //The values are the same.
                                    // Don't do anything
                                }else{
                                    //There was a change, make a @modified_tags object
                                    //[OLD, NEW]

                                    std::string s = object_history[idx-1]["@tags"][tag_key].GetString();
                                    
                                    rapidjson::Value prev_val;
                                    prev_val.SetString(rapidjson::StringRef(s));
                                    
                                    rapidjson::Value modified_tag(rapidjson::kArrayType);
                                    modified_tag.PushBack(prev_val, doc.GetAllocator());
                                    modified_tag.PushBack(tag_val, doc.GetAllocator());
                                    
                                    modified_tags.AddMember(tag_key, modified_tag, doc.GetAllocator());
                                }
                                
                            }else{
                                //This is a new tag
                                new_tags.AddMember(tag_key, tag_val, doc.GetAllocator());
                            }
                        }
                        
                        //Only add these objects to the history object if they have values.
                        if (new_tags.ObjectEmpty() == false){
                            hist_obj.AddMember("@new_tags", new_tags, doc.GetAllocator());
                        }
                        if (modified_tags.ObjectEmpty() == false){
                            hist_obj.AddMember("@modified_tags", modified_tags, doc.GetAllocator());
                        }
                        if (deleted_tags.ObjectEmpty() == false){
                            //Now figure out how to handle deleted tags?
                            hist_obj.AddMember("@deleted_tags",  new_tags, doc.GetAllocator());
                        }
                    }

                }
                
                //TODO: Remove all "@tags" objects from each hist_obj

                //Last, add history to original object
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
