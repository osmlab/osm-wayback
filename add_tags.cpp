#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <map>
#include <iterator>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"
#pragma GCC diagnostic pop

#include <osmium/geom/rapid_geojson.hpp>
#include "rocksdb/db.h"
#include <rocksdb/table.h>
#include <rocksdb/filter_policy.h>
#include "rocksdb/cache.h"


std::string make_lookup(int osm_id, int type, int version){
  return std::to_string(osm_id) + "!" + std::to_string(version) + "!" + std::to_string(type);
}

// long int make_lookup(int osm_id, int type, int version) {
//     return osm_id*1000 + type * 100 + version;
// };

//https://stackoverflow.com/questions/8473009/how-to-efficiently-compare-two-maps-of-strings-in-c
struct Pair_First_Equal {
    template <typename Pair>
    bool operator() (Pair const &lhs, Pair const &rhs) const {
        return lhs.first == rhs.first;
    }
};

template <typename Map>
bool map_compare (Map const &lhs, Map const &rhs) {
    // No predicate needed because there is operator== for pairs already.
    return lhs.size() == rhs.size()
        && std::equal(lhs.begin(), lhs.end(),
                      rhs.begin());
}
// template <typename Map>
// bool key_compare (Map const &lhs, Map const &rhs) {
//     return lhs.size() == rhs.size()
//         && std::equal(lhs.begin(), lhs.end(),
//                       rhs.begin(),
//                       Pair_First_Equal()); // predicate instance
// };

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " INDEX_DIR" << std::endl;
        std::exit(1);
    }

    std::string index_dir = argv[1];

    int feature_count = 0;
    int error_count = 0;

    rocksdb::Options options;
    options.create_if_missing = false;
    options.allow_mmap_writes = true;

    rocksdb::BlockBasedTableOptions table_opts;
    table_opts.filter_policy = std::shared_ptr<const rocksdb::FilterPolicy>(rocksdb::NewBloomFilterPolicy(10));
    // table_opts.block_cache = rocksdb::NewLRUCache(4 * 1024 * 1024 * 1024, 10);
    options.table_factory.reset(NewBlockBasedTableFactory(table_opts));
    rocksdb::DB* db;
    rocksdb::Status status = rocksdb::DB::Open(options, index_dir, &db);

    rapidjson::Document doc;
    for (std::string line; std::getline(std::cin, line);) {
        if(doc.Parse<0>(line.c_str()).HasParseError()) {
            std::cerr << "ERROR" << std::endl;
        } else {
            feature_count++;

            if(!doc.HasMember("properties")) continue;
            if(!doc["properties"]["@id"].IsInt() || !doc["properties"]["@version"].IsInt() || !doc["properties"]["@type"].IsString()) continue;

            const auto version = doc["properties"]["@version"].GetInt();
            const auto osm_id = doc["properties"]["@id"].GetInt();
            const std::string type = doc["properties"]["@type"].GetString();

            try {
                rapidjson::Value object_history(rapidjson::kArrayType);
                rapidjson::Document stored_doc;

                int osmType = 1;
                if(type == "node") osmType = 1;
                if(type == "way") osmType = 2;
                if(type == "relation") osmType = 3;

                //  Objective is to iterate ONCE through the tags and build a different object in memory that can be
                //  released after each object; then @tags object can be deleted too at each version.

                std::vector < std::map<std::string, std::string> > tag_history;

                int hist_it_idx = 0; //Can't trust the versions because they may not be contiguous

                for(int v = 1; v < version+1; v++) { //Going up to current version so that history is complete
                    const auto lookup = make_lookup(osm_id, osmType, v);
                    std::string json;
                    rocksdb::Status s= db->Get(rocksdb::ReadOptions(), lookup, &json);
                    if (s.ok()) {

                        //S is not OK for the most part;

                        if(stored_doc.Parse<0>(json.c_str()).HasParseError()) { continue; }

                        std::map<std::string,std::string> version_tags;

                        for (rapidjson::Value::ConstMemberIterator it= stored_doc["@tags"].MemberBegin(); it != stored_doc["@tags"].MemberEnd(); it++){

                            //Add the tags to the version_tags map
                            version_tags.insert( std::make_pair( it->name.GetString(), it->value.GetString() ) );

                        }
                        //We need to be careful ^ about order? How does order matter here?
                        tag_history.push_back(version_tags);

                        //It's the first version
                        if (hist_it_idx == 0){

                            //Is this the most efficient way? we just need to rename it from @tags to @new_tags
                            stored_doc.AddMember("@new_tags", stored_doc["@tags"], stored_doc.GetAllocator());
                            stored_doc.RemoveMember("@tags");

                        }else{
                            //Both v and v-1 tags are mapped in memory, so we can do map comparison

                            //Check if they are exactly the same:
                            if ( map_compare( tag_history[hist_it_idx-1], tag_history[hist_it_idx] ) ){
                                //Tags have not changed at all
                                //TODO: Delete @tags (after debugging)
                                // ^ I really hope this is working properly
                            }else{
                                //There has been one of 3 changes:
                                //1. New tags
                                //2. Mod tags
                                //3. Del tags

                                //Trying to wrap this all into ONE iteration.
                                typedef std::map<std::string,std::string> StringStringMap;
                                StringStringMap::iterator pos;

                                rapidjson::Value mod_tags(rapidjson::kObjectType);
                                rapidjson::Value new_tags(rapidjson::kObjectType);

                                for (pos = tag_history[hist_it_idx].begin(); pos != tag_history[hist_it_idx].end(); ++pos) {

                                    //First, check if the current key exists in the previous entry:
                                    std::map<std::string,std::string>::iterator search = tag_history[hist_it_idx-1].find(pos->first);

                                    if (search == tag_history[hist_it_idx-1].end()) {
                                        //Not found, so it's a new tag
                                        rapidjson::Value new_key(rapidjson::StringRef(pos->first));
                                        rapidjson::Value new_val(rapidjson::StringRef(pos->second));
                                        new_tags.AddMember(new_key, new_val, stored_doc.GetAllocator());

                                    }else {
                                        //It exists, check if it's the same, if not, it's a modified tag
                                        if( pos->second != search->second) {
                                            rapidjson::Value prev_val(rapidjson::StringRef(search->second));

                                            rapidjson::Value new_val(rapidjson::StringRef(pos->second));
                                            rapidjson::Value key(rapidjson::StringRef(pos->first));

                                            rapidjson::Value modified_tag(rapidjson::kArrayType);
                                            modified_tag.PushBack(prev_val, stored_doc.GetAllocator());
                                            modified_tag.PushBack(new_val, stored_doc.GetAllocator());
                                            mod_tags.AddMember(key, modified_tag, stored_doc.GetAllocator());
                                        }
                                        //We've dealt with it, so now erase it from the previous entry
                                        tag_history[hist_it_idx-1].erase(search->first);
                                    }

                                }
                                //If we have modified or new tags, add them
                                if(mod_tags.ObjectEmpty()==false){
                                    stored_doc.AddMember("@mod_tags", mod_tags, stored_doc.GetAllocator());
                                }
                                if(new_tags.ObjectEmpty()==false){
                                    stored_doc.AddMember("@new_tags", new_tags, stored_doc.GetAllocator());
                                }

                                //Since we've deleted keys from above, if there are any values left in the map, then we save those as deleted keys...

                                if(tag_history[hist_it_idx-1].empty() == false){
                                    rapidjson::Value del_tags(rapidjson::kObjectType);
                                    for (pos = tag_history[hist_it_idx-1].begin(); pos != tag_history[hist_it_idx-1].end(); ++pos) {
                                        rapidjson::Value del_key(rapidjson::StringRef(pos->first));
                                        rapidjson::Value del_val(rapidjson::StringRef(pos->second));
                                        del_tags.AddMember(del_key, del_val, stored_doc.GetAllocator());
                                    }
                                    stored_doc.AddMember("@del_tags", del_tags, stored_doc.GetAllocator());
                                }
                            }
                        }
                        hist_it_idx++;

                        //Save the new object into the object history
                        object_history.PushBack(stored_doc, stored_doc.GetAllocator());

                    } else {
                        error_count++;
                        continue;
                    }
                }

                //Last, add history to original object
                doc["properties"].AddMember("@history", object_history, doc.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);
                std::cout << buffer.GetString() << std::endl;

                if(feature_count%10000==0){
                  std::cerr << "\rProcessed: " << (feature_count/1000) << " K features";
                }

            } catch (const std::exception& ex) {
                std::cerr<< ex.what() << std::endl;
                continue;
            }
            //TODO what happens if there is no history? does this still get written?
        }
    }
    delete db;
    std::cerr << "\rProcessed: " << feature_count << " features successfully, with " << error_count << " errors."<<std::endl;
}
