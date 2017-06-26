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

// Allow any format of input files (XML, PBF, ...)
#include <osmium/io/any_input.hpp>
#include  <osmium/osm/types.hpp>

// We want to use the handler interface
#include <osmium/handler.hpp>

// For osmium::apply()
#include <osmium/visitor.hpp>

#include "rocksdb/db.h"


class TagStoreHandler : public osmium::handler::Handler {
    rocksdb::DB* m_db;

    //TODO: Yes this is stupid
    void store_tags(const std::string lookup, const osmium::OSMObject& object) {
        const osmium::TagList& tags = object.tags();
        std::ostringstream os;
        int count = 0;
        for (const osmium::Tag& tag : tags) {
            os << "\"" << tag.key() << "\":\"" << tag.value() << "\",";
            count += 1;
        }
        const std::string s = os.str();
        rocksdb::Status stat = m_db->Put(rocksdb::WriteOptions(), lookup, s);
        std::cout << lookup << " " << count << std::endl;
    }

public:
    TagStoreHandler(rocksdb::DB* db) : m_db(db) {}

    // Nodes can be tagged amenity=pub.
    void node(const osmium::Node& node) {
        const auto lookup = "node!" + std::to_string(node.id()) + "!" + std::to_string(node.version());
        // const auto lookup = "node!" + std::to_string(node.id());
        if (node.tags().empty()) {
            return;
        }
        store_tags(lookup, node);
    }

    // Ways can be tagged amenity=pub, too (typically buildings).
    void way(const osmium::Way& way) {
        const auto lookup = "way!" + std::to_string(way.id()) + "!" + std::to_string(way.version());
        if (way.tags().empty()) {
            return;
        }
        store_tags(lookup, way);
    }

};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " OSMFILE\n";
        std::exit(1);
    }

    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, "/tmp/testdb", &db);

    // Construct the handler defined above
    TagStoreHandler names_handler{db};

    // Initialize the reader with the filename from the command line and
    // tell it to only read nodes and ways. We are ignoring multipolygon
    // relations in this simple example.
    osmium::io::Reader reader{argv[1], osmium::osm_entity_bits::node | osmium::osm_entity_bits::way};

    // Apply input data to our own handler
    osmium::apply(reader, names_handler);
}

