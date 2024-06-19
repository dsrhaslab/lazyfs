#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include "pebblesdb/db.h"
#include "pebblesdb/options.h"

using namespace std;

/**
 * @brief Returns number of matching keys.
*/
int check_pairs (map<string,string> &pairs, int values) {
    int sum = 0;
    for (unsigned int i = 0; i < values; i++) { 
        if (pairs.count("Key" + to_string(i))) sum++;
    }
    return sum;
}

//argv[1] - data directory
//argv[2] - number of values in the db
int main(int argc, char** argv) {
    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, argv[1], &db);  

    if (false == status.ok()) {
	    cerr << "[error creating db]: " << status.ToString() << endl;
	    return -1;
    }

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());

    map<string,string> pairs;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        pairs[it->key().ToString()] = it->value().ToString();
        //cout << it->key().ToString() << " : " << it->value().ToString() << endl;
    }

    if (false == it->status().ok()) {
        cerr << "An error was found while reading the database " << endl;
        cerr << it->status().ToString() << endl;
    }

    int nkeys = check_pairs(pairs, atoi(argv[2]));
    cout << "There are " << nkeys << " pairs in the database from " << atoi(argv[2]) << " inserted" << endl;

    delete it;    
    return 0;
}
