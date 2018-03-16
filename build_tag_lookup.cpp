/*

  Build Tag History Database

  (Based on osmium_pub_names example)

*/

#include <cstdlib>  // for std::exit
#include <cstring>  // for std::strncmp
#include <iostream> // for std::cout, std::cerr
#include <sstream>
#include <chrono>

#include <osmium/io/any_input.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>

#include "db.hpp"

class ObjectStoreHandler : public osmium::handler::Handler {
    ObjectStore* m_store;

public:
    ObjectStoreHandler(ObjectStore* store) : m_store(store) {}
    long node_count = 0;
    int way_count = 0;
    int rel_count = 0;
    void node(const osmium::Node& node) {
        node_count += 1;
        m_store->store_node(node);
    }

    void way(const osmium::Way& way) {
        m_store->store_way(way);
        way_count++;
    }

    void relation(const osmium::Relation& relation) {
        m_store->store_relation(relation);
        rel_count++;
    }
};

std::atomic_bool stop_progress{false};

void report_progress(const ObjectStore* store) {
    unsigned long last_nodes_count{0};
    unsigned long last_ways_count{0};
    unsigned long last_relations_count{0};
    auto start = std::chrono::steady_clock::now();

    while(true) {
        if(stop_progress) {
            auto end = std::chrono::steady_clock::now();
            auto diff = end - start;

            std::cerr << "\nProcessed " << store->stored_nodes_count << " nodes, " << store->stored_ways_count << " ways, " << store->stored_relations_count << " relations in " << std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;
            break;
        }

        auto diff_nodes_count = store->stored_nodes_count - last_nodes_count;
        auto diff_ways_count = store->stored_ways_count - last_ways_count;
        auto diff_relations_count = store->stored_relations_count - last_relations_count;

        std::cerr << "\rProcessed " << store->stored_nodes_count / 1000000 << "M nodes @ " << diff_nodes_count << " n/s | " <<
          store->stored_ways_count /1000 << "K ways @ " << diff_ways_count << " w/s | " <<
          store->stored_relations_count << " rels @ " << diff_relations_count << "   ";


        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        last_nodes_count += diff_nodes_count;
        last_ways_count += diff_ways_count;
        last_relations_count += diff_relations_count;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " INDEX_DIR OSMFILE" << std::endl;
        std::exit(1);
    }

    std::string index_dir = argv[1];
    std::string osm_filename = argv[2];

    ObjectStore store(index_dir, true);
    ObjectStoreHandler osm_object_handler(&store);

    std::thread t_progress(report_progress, &store);

    osmium::io::Reader reader{osm_filename, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way | osmium::osm_entity_bits::relation};
    osmium::apply(reader, osm_object_handler);

    stop_progress = true;
    t_progress.join();
    store.flush();
}
