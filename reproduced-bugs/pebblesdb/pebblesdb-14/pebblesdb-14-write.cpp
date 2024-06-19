#include <iostream>
#include <string>
#include "pebblesdb/db.h"
#include "pebblesdb/options.h"
#include <sstream>

using namespace std;

//argv[1] - data directory
//argv[2] - async puts
//argv[3] - 1 to do a sync put in the final, 0 to not do it async puts 
int main(int argc, char** argv) {
    leveldb::DB* db;
    leveldb::Options options;

    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, argv[1], &db);    

    int values_unsync = atoi(argv[2]);
    int value_sync = atoi(argv[3]); 

    if (false == status.ok()) {
	    cerr << "[error creating db]: " << status.ToString() << endl;
	    return 1;
    }

    leveldb::WriteOptions writeOptions;
    for (unsigned int i = 0; i < values_unsync; i++) {
        ostringstream keyStream;
        keyStream << "Key" << i;

        ostringstream valueStream;
        valueStream << i;

        db->Put(writeOptions, keyStream.str(), valueStream.str());
    }

    if (value_sync == 1) {
	    ostringstream keyStream;
	    keyStream << "Key" << values_unsync;

	    ostringstream valueStream;
	    valueStream << values_unsync;
	     
	    writeOptions.sync = true;
	    db->Put(writeOptions,keyStream.str(),valueStream.str());
    }

    return 0;
}
