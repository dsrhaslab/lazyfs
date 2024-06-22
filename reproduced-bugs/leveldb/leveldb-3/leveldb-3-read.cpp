#include <cassert>
#include <iostream>
#include "leveldb/db.h"

//argv[1] - data directory
int main (int argc, char** argv) {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = false;
    options.paranoid_checks = false; //both true and false work
    
    leveldb::ReadOptions read_options;
    //read_options.verify_checksums = true;

    leveldb::Status status = leveldb::DB::Open(options, argv[1], &db);
    if (!status.ok()) {
        std::cerr << "[open error] "<< status.ToString() << std::endl;
        return 1;
    }

    std::string value;

    // -------------------- Read k1 ----------------------------------
    status = db->Get(read_options, "k1", &value);
    if (!status.ok()) {
        std::cerr << "[read k1 error] " << status.ToString() << std::endl;
    } else {
        std::cout << "k1: " << value << std::endl;
        assert(value == "key1");
    }

    // -------------------- Read k2 ----------------------------------
    status = db->Get(read_options, "k2", &value);
    if (!status.ok()) {
        std::cerr << "[read k2 error] " << status.ToString() << std::endl;    
    } else {
        std::cout << "k2: " << value << std::endl;
        assert(value == "key2");
    }

    delete db;
    return 0;
}
