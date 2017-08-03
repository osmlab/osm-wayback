/*

  Build Tag History Database

  (Based on osmium_pub_names example)

*/

#include <cstdlib>  // for std::exit
#include <cstring>  // for std::strncmp
#include <iostream> // for std::cout, std::cerr
#include <sstream>

#include <osmium/io/any_input.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>

#include "db.hpp"

class TagStoreHandler : public osmium::handler::Handler {
    TagStore* m_store;

public:
    TagStoreHandler(TagStore* store) : m_store(store) {}
    long node_count = 0;
    int way_count = 0;
    int rel_count = 0;
    void node(const osmium::Node& node) {
        node_count += 1;
        const auto lookup = make_lookup(node.id(), 1, node.version());
        m_store->store_tags(lookup, node);
        //Status update?
        if ( node_count % 1000000 == 0){
            std::cerr << "\rProcessed: " << node_count/1000000 << " M nodes";
        }
    }

    void way(const osmium::Way& way) {
        const auto lookup = make_lookup(way.id(), 2, way.version());
        m_store->store_tags(lookup, way);

        if ( way_count % 10000 == 0)
        {
          if (way_count == 0){
            std::cerr << "\rProcessed: " << node_count << " nodes" << std::endl;
          }
            std::cerr << "\rProcessed: " << way_count/1000 << " K ways                   ";
        }
        way_count++;
    }

    void relation(const osmium::Relation& relation) {
        const auto lookup = make_lookup(relation.id(), 3, relation.version());
        m_store->store_tags(lookup, relation);

        if ( rel_count % 10000 == 0)
        {
          if (rel_count == 0)
          {
            std::cerr << "\rProcessed: " << way_count << " ways" << std::endl;
          }
            std::cerr << "\rProcessed: " << rel_count/1000 << " K relations                   ";
        }
        rel_count++;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " INDEX_DIR OSMFILE" << std::endl;
        std::exit(1);
    }

    std::string index_dir = argv[1];
    std::string osm_filename = argv[2];

    TagStore store(index_dir);
    TagStoreHandler tag_handler(&store);

    osmium::io::Reader reader{osm_filename, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way | osmium::osm_entity_bits::relation};
    osmium::apply(reader, tag_handler);

    //TODO: Put status updates down here and not in the middle of processing?
}
