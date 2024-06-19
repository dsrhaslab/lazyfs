#include <cstdlib>
#include <leveldb/db.h>

//argv[1] - data directory
int main(int argc, char** argv) {
        leveldb::Options options;
        options.paranoid_checks = true;

        leveldb::Status s = leveldb::RepairDB(argv[1],options);

        return s.ok()?EXIT_SUCCESS:EXIT_FAILURE; 
}
