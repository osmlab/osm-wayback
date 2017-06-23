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

class TagReadHandler : public osmium::handler::Handler {
    rocksdb::DB* m_db;

    //TODO: Yes this is stupid
    void read_tags(const std::string lookup, const osmium::OSMObject& object) {
        std::string value;
        rocksdb::Status s= m_db->Get(rocksdb::ReadOptions(), lookup, &value);
        if (s.ok()) {
            std::cout << value;
        }
    }

public:
    TagReadHandler(rocksdb::DB* db) : m_db(db) {}

    // Nodes can be tagged amenity=pub.
    void node(const osmium::Node& node) {
        const auto lookup = "node!" + std::to_string(node.id()) + "!" + std::to_string(node.version());
        read_tags(lookup, node);
    }

    // Ways can be tagged amenity=pub, too (typically buildings).
    void way(const osmium::Way& way) {
        const auto lookup = "way!" + std::to_string(way.id()) + "!" + std::to_string(way.version());
        read_tags(lookup, way);
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
    TagReadHandler names_handler{db};

    // Initialize the reader with the filename from the command line and
    // tell it to only read nodes and ways. We are ignoring multipolygon
    // relations in this simple example.
    osmium::io::Reader reader{argv[1], osmium::osm_entity_bits::node | osmium::osm_entity_bits::way};

    // Apply input data to our own handler
    osmium::apply(reader, names_handler);
}

