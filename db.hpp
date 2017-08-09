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

#include <osmium/osm/types.hpp>
#include <osmium/visitor.hpp>

#include <chrono>

const std::string make_lookup(const long long int osm_id, const int version){
  return std::to_string(osm_id) + "!" + std::to_string(version);
}

class TagStore {
    rocksdb::DB* m_db;
    rocksdb::ColumnFamilyHandle* m_cf_ways;
    rocksdb::ColumnFamilyHandle* m_cf_nodes;
    rocksdb::ColumnFamilyHandle* m_cf_relations;
    rocksdb::WriteOptions m_write_options;

public:
    unsigned long empty_objects_count{0};
    unsigned long stored_tags_count{0};

    unsigned long stored_nodes_count{0};
    unsigned long stored_ways_count{0};
    unsigned long stored_relations_count{0};

    unsigned long stored_objects_count() {
        return stored_nodes_count + stored_ways_count + stored_relations_count;
    }

    TagStore(const std::string index_dir, const bool create) {
        rocksdb::Options db_options;
        db_options.allow_mmap_writes = false;
        db_options.max_background_flushes = 4;
        db_options.PrepareForBulkLoad();

        m_write_options = rocksdb::WriteOptions();
        m_write_options.sync = false;

        rocksdb::BlockBasedTableOptions table_options;
        table_options.filter_policy = std::shared_ptr<const rocksdb::FilterPolicy>(rocksdb::NewBloomFilterPolicy(10));
        db_options.table_factory.reset(NewBlockBasedTableFactory(table_options));

        rocksdb::Status s;

        if(create) {
            // always clear out the previous tag index first
            rocksdb::DestroyDB(index_dir, db_options);
            db_options.create_if_missing = true;
            s = rocksdb::DB::Open(db_options, index_dir, &m_db);
            s = m_db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "nodes", &m_cf_nodes);
            assert(s.ok());
            s = m_db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "ways", &m_cf_ways);
            assert(s.ok());
            s = m_db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "relations", &m_cf_relations);
            assert(s.ok());
        } else {
            db_options.error_if_exists = false;
            db_options.create_if_missing = false;
            std::cout << "Open without create";
            // open DB with two column families
            std::vector<rocksdb::ColumnFamilyDescriptor> column_families;
            // have to open default column family
            column_families.push_back(rocksdb::ColumnFamilyDescriptor(
            rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()));
            // open the new one, too
            column_families.push_back(rocksdb::ColumnFamilyDescriptor( "nodes", rocksdb::ColumnFamilyOptions()));
            column_families.push_back(rocksdb::ColumnFamilyDescriptor( "ways", rocksdb::ColumnFamilyOptions()));
            column_families.push_back(rocksdb::ColumnFamilyDescriptor( "relations", rocksdb::ColumnFamilyOptions()));

            std::vector<rocksdb::ColumnFamilyHandle*> handles;

            s = rocksdb::DB::Open(db_options, index_dir, column_families, &handles, &m_db);
            assert(s.ok());

            m_cf_nodes = handles[1];
            m_cf_ways = handles[2];
            m_cf_relations = handles[3];
        }
    }
  rocksdb::Status get_tags(const long int osm_id, const int osm_type, const int version, std::string* json_value) {
        const auto lookup = make_lookup(osm_id, version);
        if(osm_type== 0) {
            return m_db->Get(rocksdb::ReadOptions(), m_cf_nodes, lookup, json_value);
        } else if (osm_type == 1) {
            return m_db->Get(rocksdb::ReadOptions(), m_cf_ways, lookup, json_value);
        } else {
            return m_db->Get(rocksdb::ReadOptions(), m_cf_relations, lookup, json_value);
        }
    }

    void store_tags(const osmium::Way& way) {
        store_tags(way, m_cf_ways);
        stored_ways_count++;
    }

    void store_tags(const osmium::Node& node) {
        store_tags(node, m_cf_nodes);
        stored_nodes_count++;
    }

    void store_tags(const osmium::Relation& relation) {
        store_tags(relation, m_cf_relations);
        stored_relations_count++;
    }

    void store_tags(const osmium::OSMObject& object, rocksdb::ColumnFamilyHandle* cf) {
        const auto lookup = make_lookup(object.id(), object.version());
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

        rocksdb::Status stat = m_db->Put(m_write_options, cf, lookup, buffer.GetString());
    }

    void m_flush_family(const std::string type, rocksdb::ColumnFamilyHandle* cf) {
        auto start = std::chrono::steady_clock::now();
        std::cerr << "Flushing " << type << std::endl;
        m_db->Flush(rocksdb::FlushOptions{}, cf);
        auto end = std::chrono::steady_clock::now();
        auto diff = end - start;
        std::cerr << "Flushed " << type << " in " << std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


        start = std::chrono::steady_clock::now();
        std::cerr << "Compacting " << type << std::endl;
        m_db->CompactRange(rocksdb::CompactRangeOptions{}, cf, nullptr, nullptr);
        end = std::chrono::steady_clock::now();
        diff = end - start;
        std::cerr << "Compacted " << type << " in " << std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;
    }

    void flush() {
        m_flush_family("nodes", m_cf_nodes);
        m_flush_family("ways", m_cf_ways);
        m_flush_family("relations", m_cf_relations);
    }
};
