/*

  EXAMPLE osmium_pub_names

  Show the names and addresses of all pubs found in an OSM file.

  DEMONSTRATES USE OF:
  * file input
  * your own handler
  * access to tags

  SIMPLER EXAMPLES you might want to understand first:
  * osmium_read
  * osmium_count

  LICENSE
  The code in this example file is released into the Public Domain.

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
        rapidjson::Document::AllocatorType& a = doc.GetAllocator();
        doc.SetObject();

        doc.AddMember("timestamp", object.timestamp().to_iso(), a);
        doc.AddMember("deleted", object.deleted(), a);
        doc.AddMember("user", std::string{object.user()}, a);
        doc.AddMember("uid", object.uid(), a);
        doc.AddMember("changeset", object.changeset(), a);

        const osmium::TagList& tags = object.tags();
        int count = 0;
        for (const osmium::Tag& tag : tags) {
            rapidjson::Value key(rapidjson::StringRef(tag.key()));
            rapidjson::Value value(rapidjson::StringRef(tag.value()));
            doc.AddMember(key, value, a);
            count += 1;
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        rocksdb::Status stat = m_db->Put(rocksdb::WriteOptions(), std::to_string(lookup), buffer.GetString());
        std::cout << lookup << " " << count << std::endl;
    }

public:
    TagStoreHandler(rocksdb::DB* db) : m_db(db) {}

    // Nodes can be tagged amenity=pub.
    void node(const osmium::Node& node) {
        const auto lookup = make_lookup(node.id(), 1, node.version());
        if (node.tags().empty()) {
            return;
        }
        store_tags(lookup, node);
    }

    void way(const osmium::Way& way) {
        const auto lookup = make_lookup(way.id(), 2, way.version());
        if (way.tags().empty()) {
            return;
        }
        store_tags(lookup, way);
    }

    void relation(const osmium::Relation& relation) {
        const auto lookup = make_lookup(relation.id(), 3, relation.version());
        if (relation.tags().empty()) {
            return;
        }
        store_tags(lookup, relation);
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

