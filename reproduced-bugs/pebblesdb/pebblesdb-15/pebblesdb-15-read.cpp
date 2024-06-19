#include <iostream>
#include <fstream>
#include <iterator>
#include <map>
#include "pebblesdb/db.h"

using namespace std;

map<string,string> get_pairs (string file_name) {
    ifstream file(file_name); 
    string content;
    map<string,string> pairs;

    if (!file.is_open()) {
        cerr << "Error opening file!" << endl;
    } else {
        while (file) {
            getline(file,content);
            int delimeter_pos = content.find(":");
            string key = content.substr(0,delimeter_pos);
            string value = content.substr(delimeter_pos+1);
            pairs[key] = value;
        }
        file.close();
    }
    return pairs;
}

//argv[1] - data directory
//argv[2] - pairs path
int main (int argc, char** argv) {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = false;
    options.paranoid_checks = true;
    
    leveldb::ReadOptions read_options;
    read_options.verify_checksums = true;

    leveldb::Status status = leveldb::DB::Open(options, argv[1], &db);
    if (!status.ok()) {
        cerr << "[open error] "<< status.ToString() << endl;
        return 1;
    }

    string file_name = "~/leveldb-6/pairs.txt";
    if (argc > 2) file_name = argv[2]; 

    map<string,string> pairs = get_pairs(argv[2]);
    if (pairs.empty()) {
        cerr << "Something went wrong reading the pairs." << endl;
        return 1;
        
    } else {
        map<string, string>::iterator it;

        for (it = pairs.begin(); it != pairs.end(); it++) {
            string value;
            status = db->Get(read_options, it->first, &value);
            
            if (!status.ok()) {
                cerr << "[read " << it->first << " error] " << status.ToString() << endl;
            } else {
                if (it->second == value) cout << it->first << " corresponds to expectation." << endl;
                else {
                    cerr << it->first << " does not correspond to expectation." << endl;
                }
            }
        }
    }

    delete db;
    return 0;
}
