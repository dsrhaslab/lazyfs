#ifndef CACHE_CONFIG_HPP
#define CACHE_CONFIG_HPP

#include <string>
#include <sys/types.h>

using namespace std;

namespace cache::config {

class Config {

  private:
    void setup_config_by_size (size_t prealloc_bytes, int nr_blocks_per_page);
    void setup_config_manually (size_t io_blk_sz, size_t pg_sz, size_t nr_pgs);

  public:
    // defaults values
    bool is_default_config  = true;
    size_t CACHE_NR_PAGES   = 5;
    size_t CACHE_PAGE_SIZE  = 4096;
    size_t IO_BLOCK_SIZE    = 4096;
    size_t DISK_SECTOR_SIZE = 512;
    int APPLY_LRU_EVICTION  = false;
    string FIFO_PATH        = "faults.fifo";

    // size must be a power of 2, otherwise it will be rounded
    // to the next power of 2
    Config () {};
    Config (size_t io_blk_sz, size_t pg_sz, size_t nr_pgs);
    Config (size_t prealloc_bytes, int nr_blocks_per_page);
    ~Config ();
    void set_eviction_flag (bool flag);
    void print_config ();
    void load_config (string filename);
};

} // namespace cache::config

#endif