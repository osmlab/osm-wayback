#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include <osmium/io/any_input.hpp>
#include <osmium/osm/types.hpp>

#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"
#pragma GCC diagnostic pop

#include <osmium/geom/rapid_geojson.hpp>

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
	for (std::string line; std::getline(std::cin, line);) {
        rapidjson::Document doc;
        if(doc.Parse<0>(line.c_str()).HasParseError()) {
            std::cout << "ERROR" << std::endl;
        } else {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            std::cout << "json coming" << buffer.GetString() << std::endl;
        }
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

