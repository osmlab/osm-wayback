#pragma once

#include <protozero/pbf_writer.hpp>
#include <protozero/pbf_reader.hpp>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"

#include <osmium/osm/types.hpp>

namespace jsonencoding {

/*

    JSON Object Encoding
    ====================

    These functions convert osmium objects to json objects (rapidjson::Document)

    These are primarily used to add json strings to rocksdb so will likely be deprecated shortly.

*/

    /*
        Record Node Locations with basic properties
    */

    bool encode_location_json(const osmium::Node& node, rapidjson::Document& doc){

        rapidjson::Document::AllocatorType& a = doc.GetAllocator();
        rapidjson::Value thisNode(rapidjson::kObjectType);

        if( !node.deleted() ){
            try{
                rapidjson::Value coordinates(rapidjson::kArrayType);
                coordinates.PushBack(node.location().lon(), a);
                coordinates.PushBack(node.location().lat(), a);
                thisNode.AddMember("p", coordinates, a); //p for point

            } catch (const osmium::invalid_location& ex) {
                //Catch invlid locations, not sure why this would happen... but it could
                //std::cerr<< ex.what() << std::endl;
            }
        }

        rapidjson::Value changesetKey;
        changesetKey.SetString(std::to_string(node.changeset()), a);// = changesetStr; //(rapidjson::StringRef(changesetStr));

        if (doc.HasMember(changesetKey)){
            if (unsigned(doc[changesetKey]["i"].GetInt()) > node.version() ){
                return true;
            }else{
                doc.RemoveMember(changesetKey);
            }
        }

        thisNode.AddMember("t", uint32_t(node.timestamp()), a);
        thisNode.AddMember("c", node.changeset(), a);
        thisNode.AddMember("i", node.version(), a);   //i for iteration (version)
        thisNode.AddMember("h", std::string{node.user()}, a); //handle
        thisNode.AddMember("u", node.uid(), a);

        doc.AddMember(changesetKey, thisNode, a);

        return true;
    }

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
        doc.AddMember("h", std::string{object.user()}, a); //handle
        doc.AddMember("u", object.uid(), a);

        return doc;
    }

    /*
      Extract main OSM properties from the object
    */
    rapidjson::Document extract_osm_properties(const osmium::OSMObject& object){
        rapidjson::Document doc;
        doc.SetObject();

        rapidjson::Document::AllocatorType& a = doc.GetAllocator();

        doc.AddMember("t", uint32_t(object.timestamp()), a);
        doc.AddMember("v", object.visible(), a);
        doc.AddMember("h", std::string{object.user()}, a); //handle
        doc.AddMember("u", object.uid(), a);
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
