#pragma once

#include <protozero/pbf_writer.hpp>
#include <protozero/pbf_reader.hpp>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"

#include <osmium/osm/types.hpp>
#include <map>

namespace osmwayback {

/*
    PBF Object Encoding
    ===================

    These functions encode osmium objects into simplified PBF messages to store in rocksdb

    Message Keys for Encoding:
    1. Timestamp
    2. Chnageset
    3. version
    4. User ID
    5. User (String)
    6. Visible (bool)
    7. Deleted (bool)
    10: Tags
*/

    const std::string encode_node(const osmium::Node& node) {
        std::string data;
        protozero::pbf_writer encoder(data);

        encoder.add_fixed64(1, static_cast<int>(node.timestamp().seconds_since_epoch()));
        encoder.add_uint32(2, node.changeset());
        encoder.add_uint32(3, node.version());
        encoder.add_uint32(4, node.uid());
        encoder.add_string(5, node.user());

        encoder.add_bool(6, node.visible());
        encoder.add_bool(7, node.deleted());

        //Add node coordinates
        //If deleted, will always throw invalid location exception
        if( !node.deleted() ){
            try{
                encoder.add_double(8, node.location().lon());
                encoder.add_double(9, node.location().lat());
            } catch (const osmium::invalid_location& ex) {
                //Catch invlid locations, not sure why this would happen... but it could
                std::cerr<< ex.what() << std::endl;
            }
        }

        //Add the tags
        const osmium::TagList& tags = node.tags();
        for (const osmium::Tag& tag : tags) {
          encoder.add_string(10, tag.key());
          encoder.add_string(10, tag.value());
      }
      return data;
    }

    const std::string encode_way(const osmium::Way& way) {
        std::string data;
        protozero::pbf_writer encoder(data);

        encoder.add_fixed64(1, static_cast<int>(way.timestamp().seconds_since_epoch()));
        encoder.add_uint32(2, way.changeset());
        encoder.add_uint32(3, way.version());
        encoder.add_uint32(4, way.uid());
        encoder.add_string(5, way.user());

        encoder.add_bool(6, way.visible());
        encoder.add_bool(7, way.deleted());

        //Add node references: TODO: Optimize
        std::vector<int64_t> noderefs;
        for (const osmium::NodeRef& nr : way.nodes()) {
            noderefs.push_back(nr.ref());
        }
        encoder.add_packed_int64(8,  noderefs.begin(), noderefs.end());

        //Add the tags
        const osmium::TagList& tags = way.tags();
        for (const osmium::Tag& tag : tags) {
          encoder.add_string(10, tag.key());
          encoder.add_string(10, tag.value());
      }
      return data;
    }


/*
    PBF Object Decoding
    ===================

    These functions decode PBF messages from rocksdb INTO json objects

*/

    // Decode PBF_Node as JSON Object (in place (?) )
    void decode_node(std::string data, rapidjson::Document* doc) {
        protozero::pbf_reader message(data);

        //Initialize the object (object is defined in add_tags)
        doc->SetObject();
        rapidjson::Document::AllocatorType& a = doc->GetAllocator();

        std::string previous_key{};
        rapidjson::Value object_tags(rapidjson::kObjectType);
        rapidjson::Value coordinates(rapidjson::kArrayType);
        while (message.next()) {
            switch (message.tag()) {
                case 1:
                    doc->AddMember("@timestamp", message.get_fixed64(), a);
                    break;
                case 2:
                    doc->AddMember("@changeset", message.get_uint32(), a);
                    break;
                case 3:
                    doc->AddMember("@version", message.get_uint32(), a);
                    break;
                case 4:
                    doc->AddMember("@uid", message.get_uint32(), a);
                    break;
                case 5:
                    doc->AddMember("@user", message.get_string(), a);
                    break;
                case 6:
                    doc->AddMember("@visible", message.get_bool(), a);
                    break;
                case 7:
                    doc->AddMember("@deleted", message.get_bool(), a);
                    break;
                case 8:
                      coordinates.PushBack(message.get_double(),a);
                      break;
                case 9:
                      coordinates.PushBack(message.get_double(),a);
                      break;
                case 10:
                //Tags
                    if (previous_key.empty()) {
                        previous_key = message.get_string();
                    } else {
                        rapidjson::Value key(previous_key.c_str(), a);
                        rapidjson::Value value(message.get_string(), a);

                        object_tags.AddMember(key, value, a);
                        previous_key = "";
                    }
                    break;
                default:
                    message.skip();
            }
        }

        if (!coordinates.ObjectEmpty() ){
            doc->AddMember("@coordinates",coordinates,a);
        }

        if ( !object_tags.ObjectEmpty() ){
            doc->AddMember("@tags",object_tags, a);
        }
    }

    // Decode PBF Way as JSON Object (in place (?) )
    void decode_way(std::string data, rapidjson::Document* doc) {
        protozero::pbf_reader message(data);

        //Initialize the object (object is defined in add_tags)
        doc->SetObject();
        rapidjson::Document::AllocatorType& a = doc->GetAllocator();

        rapidjson::Value noderefs(rapidjson::kArrayType);

        std::string previous_key{};
        rapidjson::Value object_tags(rapidjson::kObjectType);

        protozero::iterator_range<protozero::pbf_reader::const_int64_iterator> nodeIDs;

        while (message.next()) {
            switch (message.tag()) {
                case 1:
                    doc->AddMember("@timestamp", message.get_fixed64(), a);
                    break;
                case 2:
                    doc->AddMember("@changeset", message.get_uint32(), a);
                    break;
                case 3:
                    doc->AddMember("@version", message.get_uint32(), a);
                    break;
                case 4:
                    doc->AddMember("@uid", message.get_uint32(), a);
                    break;
                case 5:
                    doc->AddMember("@user", message.get_string(), a);
                    break;
                case 6:
                    doc->AddMember("@visible", message.get_bool(), a);
                    break;
                case 7:
                    doc->AddMember("@deleted", message.get_bool(), a);
                    break;
                case 8:
                    //Node refs
                    nodeIDs = message.get_packed_int64();
                    for (auto nr : nodeIDs) {
                        noderefs.PushBack(nr,a);
                    }
                    break;
                case 10:
                    //Tags
                    if (previous_key.empty()) {
                        previous_key = message.get_string();
                    } else {
                        rapidjson::Value key(previous_key.c_str(), a);
                        rapidjson::Value value(message.get_string(), a);

                        object_tags.AddMember(key, value, a);
                        previous_key = "";
                    }
                    break;
                default:
                    message.skip();
            }
        }

        if ( !noderefs.ObjectEmpty() ){
            doc->AddMember("@nodes",noderefs, a);
        }

        if ( !object_tags.ObjectEmpty() ){
            doc->AddMember("@tags",object_tags, a);
        }
    }



/*

    JSON Object Encoding
    ====================

    These functions convert osmium objects to json objects (rapidjson::Document)

    These are primarily used to add json strings to rocksdb so will likely be deprecated shortly.

*/

    /*
      Extract only primary properties
    */
    rapidjson::Document extract_primary_properties(const osmium::OSMObject& object){
        rapidjson::Document doc;
        doc.SetObject();

        rapidjson::Document::AllocatorType& a = doc.GetAllocator();

        // doc.AddMember("t", object.timestamp().to_iso(), a);
        doc.AddMember("t", uint32_t(object.timestamp()), a);
        doc.AddMember("c", object.changeset(), a);
        doc.AddMember("i", object.version(), a);   //i for iteration (version)

        return doc;
    }

    /*
      Extract main OSM properties from the object
    */
    rapidjson::Document extract_osm_properties(const osmium::OSMObject& object){
        rapidjson::Document doc;
        doc.SetObject();

        rapidjson::Document::AllocatorType& a = doc.GetAllocator();

        // doc.AddMember("t", object.timestamp().to_iso(), a); //ISO is helpful for debugging, but should we leave as int?
        doc.AddMember("t", uint32_t(object.timestamp()), a);
        doc.AddMember("v", object.visible(), a);
        doc.AddMember("u", std::string{object.user()}, a);
        doc.AddMember("ui", object.uid(), a);
        doc.AddMember("c", object.changeset(), a); //
        doc.AddMember("i", object.version(), a);   //i for iteration (version)

        //Extra
        if (object.deleted()){
            doc.AddMember("d", object.deleted(), a);
        }

        //Tags
        const osmium::TagList& tags = object.tags();

        rapidjson::Value object_tags(rapidjson::kObjectType);
        for (const osmium::Tag& tag : tags) {
            rapidjson::Value key(rapidjson::StringRef(tag.key()));
            rapidjson::Value value(rapidjson::StringRef(tag.value()));

            object_tags.AddMember(key, value, a);
        }
        //a for attributes
        doc.AddMember("a", object_tags, a);

        return doc;
    }
}


    // struct Changeset {
    //   uint64_t created_at;
    //   uint64_t closed_at;
    //   std::map<std::string, std::string> tags;
    // };
    //
    // const std::string encode_changeset(const osmium::Changeset& changeset) {
    //   std::string data;
    //   protozero::pbf_writer encoder(data);
    //
    //   encoder.add_fixed64(1, static_cast<int>(changeset.created_at().seconds_since_epoch()));
    //   encoder.add_fixed64(2, static_cast<int>(changeset.closed_at().seconds_since_epoch()));
    //
    //   const osmium::TagList& tags = changeset.tags();
    //   for (const osmium::Tag& tag : tags) {
    //     encoder.add_string(17, tag.key());
    //     encoder.add_string(17, tag.value());
    //   }
    //
    //   return data;
    // }
    //
    // const Changeset decode_changeset(std::string data) {
    //   protozero::pbf_reader message(data);
    //   Changeset changeset{};
    //   changeset.tags = std::map<std::string, std::string>{};
    //
    //   std::string previous_key{};
    //     while (message.next()) {
    //         switch (message.tag()) {
    //             case 1:
    //                 changeset.created_at = message.get_fixed64();
    //                 break;
    //             case 2:
    //                 changeset.closed_at = message.get_fixed64();
    //                 break;
    //             case 17:
    //                 changeset.closed_at = message.get_fixed64();
    //                 if (previous_key.empty()) {
    //                     previous_key = message.get_string();
    //                 } else {
    //                     changeset.tags[previous_key] = message.get_string();
    //                     previous_key = "";
    //                 }
    //                 break;
    //             default:
    //                 message.skip();
    //         }
    //     }
    //
    //  return changeset;
    // }
// }


// A separate object? Are there benefits of doing this?
// struct Node {
//     uint64_t timestamp;
//     int      changeset;
//     int      uid;
//     int      version;
//     std::string user;
//     bool     visible;
//     bool     deleted;
//
//     std::map<std::string, std::string> tags;
// };
