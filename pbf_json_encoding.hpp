#pragma once

#include <protozero/pbf_writer.hpp>
#include <protozero/pbf_reader.hpp>

#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"

#include <osmium/osm/types.hpp>
#include <map>

namespace osmwayback {

    struct Node {
        uint64_t timestamp;
        uint32   changeset;
        int      uid;
        int      version;
        std::string user;
        bool     visible;
        bool     deleted;

        std::map<std::string, std::string> tags;
    };

    const std::string encode_node(const osmium::Node& node) {
        std::string data;
        protozero::pbf_writer encoder(data);

        encoder.add_fixed64(1, static_cast<int>(node.timestamp().seconds_since_epoch()));
        encoder.add_uint32(2, node.changeset());
        encoder.add_uint32(3, node.version());
        encoder.add_uint32(4, node.uid());
        encoder.add_string(5, node.user());

        encoder.add_bool(6, node.visible());
        encoder.add_bool(7, node.deleted());

        //Add node coordinates, unless deleted? In which case, add what?

        //Add the tags
        const osmium::TagList& tags = node.tags();
        for (const osmium::Tag& tag : tags) {
          encoder.add_string(17, tag.key());
          encoder.add_string(17, tag.value());
      }

      return data;
    }

    const Node decode_changeset(std::string data) {
      protozero::pbf_reader message(data);
      Changeset changeset{};
      changeset.tags = std::map<std::string, std::string>{};

      std::string previous_key{};
        while (message.next()) {
            switch (message.tag()) {
                case 1:
                    changeset.created_at = message.get_fixed64();
                    break;
                case 2:
                    changeset.closed_at = message.get_fixed64();
                    break;
                case 17:
                    changeset.closed_at = message.get_fixed64();
                    if (previous_key.empty()) {
                        previous_key = message.get_string();
                    } else {
                        changeset.tags[previous_key] = message.get_string();
                        previous_key = "";
                    }
                    break;
                default:
                    message.skip();
            }
        }

     return changeset;
    }
}


    // struct Changeset {
    //   uint64_t created_at;
    //   uint64_t closed_at;
    //   std::map<std::string, std::string> tags;
    // };
    //
    // const std::string encode_changeset(const osmium::Changeset& changeset) {
    //   std::string data;
    //   protozero::pbf_writer encoder(data);
    //
    //   encoder.add_fixed64(1, static_cast<int>(changeset.created_at().seconds_since_epoch()));
    //   encoder.add_fixed64(2, static_cast<int>(changeset.closed_at().seconds_since_epoch()));
    //
    //   const osmium::TagList& tags = changeset.tags();
    //   for (const osmium::Tag& tag : tags) {
    //     encoder.add_string(17, tag.key());
    //     encoder.add_string(17, tag.value());
    //   }
    //
    //   return data;
    // }
    //
    // const Changeset decode_changeset(std::string data) {
    //   protozero::pbf_reader message(data);
    //   Changeset changeset{};
    //   changeset.tags = std::map<std::string, std::string>{};
    //
    //   std::string previous_key{};
    //     while (message.next()) {
    //         switch (message.tag()) {
    //             case 1:
    //                 changeset.created_at = message.get_fixed64();
    //                 break;
    //             case 2:
    //                 changeset.closed_at = message.get_fixed64();
    //                 break;
    //             case 17:
    //                 changeset.closed_at = message.get_fixed64();
    //                 if (previous_key.empty()) {
    //                     previous_key = message.get_string();
    //                 } else {
    //                     changeset.tags[previous_key] = message.get_string();
    //                     previous_key = "";
    //                 }
    //                 break;
    //             default:
    //                 message.skip();
    //         }
    //     }
    //
    //  return changeset;
    // }
}
