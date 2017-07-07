/*

  Build Tag History Database

  (Based on osmium_pub_names example)  

*/

#include <cstdlib>  // for std::exit
#include <cstring>  // for std::strncmp
#include <iostream> // for std::cout, std::cerr
#include <sstream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"
#pragma GCC diagnostic pop

// Allow any format of input files (XML, PBF, ...)
#include <osmium/io/any_input.hpp>
#include  <osmium/osm/types.hpp>

// We want to use the handler interface
#include <osmium/handler.hpp>

// For osmium::apply()
#include <osmium/visitor.hpp>

#include "rocksdb/db.h"
#include <rocksdb/table.h>
#include <rocksdb/filter_policy.h>

long int make_lookup(int osm_id, int type, int version) {
    return osm_id * 10000 + type * 1000 + version;
}

class TagStoreHandler : public osmium::handler::Handler {
    rocksdb::DB* m_db;

    //TODO: Yes this is stupid and slow
    void store_tags(const long int lookup, const osmium::OSMObject& object) {
        rapidjson::Document doc;

        doc.SetObject();
                
        rapidjson::Document::AllocatorType& a = doc.GetAllocator();

        doc.AddMember("@timestamp", object.timestamp().to_iso(), a); //ISO is helpful for debugging, but we should leave it
        doc.AddMember("@deleted", object.deleted(), a);
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
        }
        
        doc.AddMember("@tags", object_tags, a);
        
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        rocksdb::Status stat = m_db->Put(rocksdb::WriteOptions(), std::to_string(lookup), buffer.GetString());
    }

public:
    TagStoreHandler(rocksdb::DB* db) : m_db(db) {}
    int node_count = 0;
    int way_count = 0;
    int rel_count = 0;
    void node(const osmium::Node& node) {
        node_count += 1;
        const auto lookup = make_lookup(node.id(), 1, node.version());
            // const auto lookup = "node!" + std::to_string(node.id());
            if (node.tags().empty()) {
                return;
            }
            store_tags(lookup, node);
        //Status update?
        std::cerr << "\rProcessed: " << (node_count/1000) << " K nodes";
    }
    
    //Add something to do on the end of nodes (like flush)

    
    void way(const osmium::Way& way) {
        way_count += 1;
        const auto lookup = make_lookup(way.id(), 2, way.version());
        if (way.tags().empty()) {
            return;
        }
        store_tags(lookup, way);
        
        if ( way_count % 1000 == 0)
        {
            std::cerr << "\rProcessed: " << (way_count/1000) << " K ways                   ";
        }
    }
    
    //Add something to do on the end of ways (like flush)

    void relation(const osmium::Relation& relation) {
        rel_count += 1;
        const auto lookup = make_lookup(relation.id(), 3, relation.version());
        if (relation.tags().empty()) {
            return;
        }
        store_tags(lookup, relation);
        
        if ( rel_count % 1000 == 0)
        {
            std::cerr << "\rProcessed: " << (rel_count/1000.0) << " K relations                   ";
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " INDEX_DIR OSMFILE" << std::endl;
        std::exit(1);
    }

    std::string index_dir = argv[1];
    std::string osm_filename = argv[2];

    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    options.allow_mmap_writes = true;

    rocksdb::BlockBasedTableOptions table_opts;
    table_opts.filter_policy = std::shared_ptr<const rocksdb::FilterPolicy>(rocksdb::NewBloomFilterPolicy(10));
    options.table_factory.reset(NewBlockBasedTableFactory(table_opts));

    rocksdb::Status status = rocksdb::DB::Open(options, index_dir, &db);
    TagStoreHandler tag_handler{db};
    osmium::io::Reader reader{osm_filename, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way | osmium::osm_entity_bits::relation};
    osmium::apply(reader, tag_handler);
}

