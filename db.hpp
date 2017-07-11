#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"
#pragma GCC diagnostic pop

#include "rocksdb/db.h"
#include <rocksdb/table.h>
#include <rocksdb/filter_policy.h>
#include "rocksdb/cache.h"

const std::string make_lookup(const int osm_id, const int type, const int version){
  return std::to_string(osm_id) + "!" + std::to_string(version) + "!" + std::to_string(type);
}

class TagStore {
    rocksdb::DB* m_db;

public:
    long int empty_objects_count{0};
    long int stored_tags_count{0};
    long int stored_objects_count{0};

    TagStore(const std::string index_dir) {
        rocksdb::Options options;
        options.create_if_missing = true;
        options.allow_mmap_writes = true;

        rocksdb::BlockBasedTableOptions table_opts;
        table_opts.filter_policy = std::shared_ptr<const rocksdb::FilterPolicy>(rocksdb::NewBloomFilterPolicy(10));
        options.table_factory.reset(NewBlockBasedTableFactory(table_opts));

        rocksdb::DB::Open(options, index_dir, &m_db);
    }

    rocksdb::Status get_tags(const std::string lookup, std::string* json_value) {
        return m_db->Get(rocksdb::ReadOptions(), lookup, json_value);
    }

    void store_tags(const std::string lookup, const osmium::OSMObject& object) {
        if (object.tags().empty()) {
            empty_objects_count++;
            return;
        }

        rapidjson::Document doc;
        doc.SetObject();

        rapidjson::Document::AllocatorType& a = doc.GetAllocator();

        doc.AddMember("@timestamp", object.timestamp().to_iso(), a); //ISO is helpful for debugging, but we should leave it
        if (object.deleted()){
          doc.AddMember("@deleted", object.deleted(), a);
        }
        doc.AddMember("@visible", object.visible(), a);
        doc.AddMember("@user", std::string{object.user()}, a);
        doc.AddMember("@uid", object.uid(), a);
        doc.AddMember("@changeset", object.changeset(), a);
        doc.AddMember("@version", object.version(), a);

        //Ignore trying to store geometries, but if we could scale that, it'd be awesome.
        const osmium::TagList& tags = object.tags();

        rapidjson::Value object_tags(rapidjson::kObjectType);
        for (const osmium::Tag& tag : tags) {
            rapidjson::Value key(rapidjson::StringRef(tag.key()));
            rapidjson::Value value(rapidjson::StringRef(tag.value()));

            object_tags.AddMember(key, value, a);
            stored_tags_count++;
        }

        doc.AddMember("@tags", object_tags, a);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        rocksdb::Status stat = m_db->Put(rocksdb::WriteOptions(), lookup, buffer.GetString());
        stored_objects_count++;
    }
};
