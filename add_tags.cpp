#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/document.h"
#pragma GCC diagnostic pop

#include <osmium/geom/rapid_geojson.hpp>

#include "rocksdb/db.h"

int main(int argc, char* argv[]) {
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, "/tmp/testdb", &db);

    for (std::string line; std::getline(std::cin, line);) {
        rapidjson::Document doc;
        if(doc.Parse<0>(line.c_str()).HasParseError()) {
            std::cout << "ERROR" << std::endl;
        } else {
            if(!doc.HasMember("properties")) continue;
            if(!doc["properties"]["@id"].IsInt() || !doc["properties"]["@version"].IsInt() || !doc["properties"]["@type"].IsString()) continue;

            try {
                const auto osmId = doc["properties"]["@id"].GetInt();
                const auto version = doc["properties"]["@version"].GetInt();
                const std::string type = doc["properties"]["@type"].GetString();
                const auto lookup = type + "!" + std::to_string(osmId) + "!" + std::to_string(version);
                // const auto lookup = type + "!" + std::to_string(osmId);
                std::string value;
                rocksdb::Status s= db->Get(rocksdb::ReadOptions(), lookup, &value);
                if (s.ok()) {
                    std::cout << lookup << " " << value << std::endl;
                } else {
                    std::cout << "NOT FOUND " << lookup << std::endl;
                    continue;
                }
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);

                // std::cout << "json coming" << buffer.GetString() << std::endl;
            } catch (const std::exception& ex) {
                std::cerr<< ex.what() << std::endl;
                continue;
            }

        }
    }
}
