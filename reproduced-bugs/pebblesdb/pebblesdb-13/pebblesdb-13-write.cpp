#include <iostream>
#include <string>
#include <sstream>
#include "pebblesdb/db.h"

using namespace std;

//argv[1] - data directory
int main(int argc, char** argv) {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::WriteOptions write_options;
    write_options.sync = true;

    leveldb::Status status = leveldb::DB::Open(options, argv[1], &db);    

    if (false == status.ok()) {
	    cerr << "[error creating db]: " << status.ToString() << endl;
	    return 1;
    }

    //----------------------------Write k1-------------------------------------
    status = db->Put(write_options, "k1", "v1");
    if (!status.ok()) {
        std::cerr << "[write k1 error] " << status.ToString() << std::endl;
    }

    //----------------------------Write k2-------------------------------------
    db->Put(write_options, "k2", "v2");
    if (!status.ok()) {
        std::cerr << "[write k2 error] " << status.ToString() << std::endl;
    }

    return 0;
}