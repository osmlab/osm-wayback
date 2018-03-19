/*

  USAGE: cat <LINE-DELIMITED GEOJSON> add_tags <INDEX DIR>

  Reads a stream of GeoJSON objects (line-delimited) and looks up the previous
  versions of each object in the rocksdb INDEX.

  It outputs a modified, enriched version of hte object with the `@history`
  property if there is any history.

*/

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

#include "db.hpp"
#include "pbf_json_encoding.hpp"

const bool PBF_DECODING = true;

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

long int feature_count = 0;
double lookup_fail = 0.0;
int input_feature_parse_error = 0;
int no_properties = 0;
int wrong_type_of_identity_properties = 0;
int dbrocks_parse_error = 0;
long int history_count = 0;

typedef std::map<std::string,std::string> StringStringMap;
typedef std::map<std::string,std::string> VersionTags;
typedef std::vector < std::map<std::string, std::string> > TagHistoryArray;

void write_with_history_tags(ObjectStore* store, const std::string line) {
    rapidjson::Document geojson_doc;

    if(geojson_doc.Parse<0>(line.c_str()).HasParseError()) {
        std::cerr << "ERROR" << std::endl;
        input_feature_parse_error++;
        return;
    }

    // if(!geojson_doc.HasMember("properties")){
    //     no_properties++;
    //     return;
    // }

    // if( !geojson_doc["properties"]["@id"].IsInt64()    ||
    //     !geojson_doc["properties"]["@version"].IsInt() ||
    //     !geojson_doc["properties"]["@type"].IsString()    ){
    //       wrong_type_of_identity_properties++;
    //       return;
    // }

    const auto version = geojson_doc["properties"]["@version"].GetInt();
    const auto osm_id = geojson_doc["properties"]["@id"].GetInt64();
    const std::string type = geojson_doc["properties"]["@type"].GetString();

    try {
        rapidjson::Value object_history(rapidjson::kArrayType);
        rapidjson::Document stored_doc;

        int osmType = 1;

        if(type == "node")     osmType = 1;
        else if(type == "way")      osmType = 2;
        else if(type == "relation") osmType = 3;

        TagHistoryArray tag_history;

        int hist_it_idx = 0; //Can't trust the versions because they may not be contiguous

        if (version > 1){
            for(int v = 1; v <= version; v++) { //Going up to current version so that history is complete

                std::string rocksEntry;
                rocksdb::Status s = store->get_tags(osm_id, osmType, v, &rocksEntry);

                if (s.ok()) {
                    if (PBF_DECODING && osmType==1){
                        osmwayback::decode_node(rocksEntry, &stored_doc);
                    }else if (PBF_DECODING && osmType==2){
                        osmwayback::decode_way(rocksEntry, &stored_doc);
                    }else{
                        if(stored_doc.Parse<0>(rocksEntry.c_str()).HasParseError()) {
                            dbrocks_parse_error++;
                            continue;
                        }
                    }

                    /*
                        a  = tags (attributes)
                        aA = attributes added;
                        aM = attributes modified;
                        aD = attributes deleted;
                    */

                    VersionTags version_tags;

                    for (rapidjson::Value::ConstMemberIterator it= stored_doc["a"].MemberBegin(); it != stored_doc["a"].MemberEnd(); it++){

                        //Add the tags to the version_tags map
                        version_tags.insert( std::make_pair( it->name.GetString(), it->value.GetString() ) );

                    }
                    //We need to be careful ^ about order? How does order matter here?
                    tag_history.push_back(version_tags);

                    //It's the first version
                    if (hist_it_idx == 0){

                        //Is this the most efficient way? we just need to rename it from @tags to @new_tags
                        stored_doc.AddMember("aA", stored_doc["a"], geojson_doc.GetAllocator());

                    }else{

                        //Check if they are exactly the same:
                        if ( map_compare( tag_history[hist_it_idx-1], tag_history[hist_it_idx] ) ){
                            //If they are exactly the same... do nothing
                        }else{
                            //There has been one of 3 changes:
                            //1. New tags
                            //2. Mod tags
                            //3. Del tags

                            //Trying to wrap this all into ONE iteration.
                            StringStringMap::iterator pos;

                            rapidjson::Value mod_tags(rapidjson::kObjectType);
                            rapidjson::Value new_tags(rapidjson::kObjectType);

                            for (pos = tag_history[hist_it_idx].begin(); pos != tag_history[hist_it_idx].end(); ++pos) {

                                //First, check if the current key exists in the previous entry:
                                StringStringMap::iterator search = tag_history[hist_it_idx-1].find(pos->first);

                                if (search == tag_history[hist_it_idx-1].end()) {
                                    //Not found, so it's a new tag
                                    rapidjson::Value new_key(rapidjson::StringRef(pos->first));
                                    rapidjson::Value new_val(rapidjson::StringRef(pos->second));
                                    new_tags.AddMember(new_key, new_val, geojson_doc.GetAllocator());

                                }else {
                                    //It exists, check if it's the same, if not, it's a modified tag
                                    if( pos->second != search->second) {
                                        rapidjson::Value prev_val(rapidjson::StringRef(search->second));

                                        rapidjson::Value new_val(rapidjson::StringRef(pos->second));
                                        rapidjson::Value key(rapidjson::StringRef(pos->first));

                                        rapidjson::Value modified_tag(rapidjson::kArrayType);
                                        modified_tag.PushBack(prev_val, geojson_doc.GetAllocator());
                                        modified_tag.PushBack(new_val, geojson_doc.GetAllocator());
                                        mod_tags.AddMember(key, modified_tag, geojson_doc.GetAllocator());
                                    }
                                }
                            }
                            //If we have modified or new tags, add them
                            if(mod_tags.ObjectEmpty()==false){
                                stored_doc.AddMember("aM", mod_tags, geojson_doc.GetAllocator());
                            }
                            if(new_tags.ObjectEmpty()==false){
                                stored_doc.AddMember("aA", new_tags, geojson_doc.GetAllocator());
                            }

                            //Iterate over previous tags, check if any of them don't exist in this version (DEL)
                            rapidjson::Value del_tags(rapidjson::kObjectType);
                            for (pos = tag_history[hist_it_idx-1].begin(); pos != tag_history[hist_it_idx-1].end(); ++pos) {
                                if (tag_history[hist_it_idx].count(pos->first) == 0){
                                  rapidjson::Value del_key(rapidjson::StringRef(pos->first));
                                  rapidjson::Value del_val(rapidjson::StringRef(pos->second));
                                  del_tags.AddMember(del_key, del_val, geojson_doc.GetAllocator());
                                }
                            }

                            if (del_tags.ObjectEmpty() == false){
                                stored_doc.AddMember("aD", del_tags, geojson_doc.GetAllocator());
                            }
                        }
                    }
                    hist_it_idx++;
                    stored_doc.RemoveMember("a"); //We'll remove the larger attributes object because we're only keeping diffs.

                    //Save the new object into the object history
                    object_history.PushBack(stored_doc, geojson_doc.GetAllocator());

                } else {
                    lookup_fail++;
                    continue;
                }
            }//end VERSION LOOP
            //Last, add history to original object
            geojson_doc["properties"].AddMember("@history", object_history, geojson_doc.GetAllocator());
            history_count += hist_it_idx;

        }//END IF VERSION > 1

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        geojson_doc.Accept(writer);
        std::cout << buffer.GetString() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr<< ex.what() << std::endl;
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
        write_with_history_tags(&store, line);
        feature_count++;
        if(feature_count%10000==0){
          std::cerr << "\rProcessed: " << (feature_count/1000) << " K features";
        }
    }

    if(feature_count == 0) {
        std::cerr << "No features processed" << std::endl;
        std::exit(5);
    }

    std::cerr << "\n"<< feature_count << " features processed, additional history values: " << history_count << std::endl;
    std::cerr << "\t" << lookup_fail << " (" << (lookup_fail / (lookup_fail + history_count)*100) << "%) \tLookup failures"  << std::endl;
    std::cerr << "\t" << input_feature_parse_error <<  "\tInput feature parse failures"  << std::endl;
    std::cerr << "\t" << no_properties <<  "\tInput features without _properties_ object"  << std::endl;
    std::cerr << "\t" << wrong_type_of_identity_properties <<  "\tInput features with wrong property types"   << std::endl;
    std::cerr << "\t" << dbrocks_parse_error << "\tStored doc parsing failures" << std::endl;
}
