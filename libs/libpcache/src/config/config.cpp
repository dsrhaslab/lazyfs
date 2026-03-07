
/**
 * @file config.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <assert.h>
#include <cache/config/config.hpp>
#include <cache/constants/constants.hpp>
#include <math.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>

using namespace std;

namespace cache::config {

/* Configuration */

Config::Config (size_t prealloc_bytes, int nr_blocks_per_page) {
    this->setup_config_by_size (prealloc_bytes, nr_blocks_per_page);
}

Config::Config (size_t io_blk_sz, size_t pg_sz, size_t nr_pgs) {

    this->setup_config_manually (io_blk_sz, pg_sz, nr_pgs);
}

void Config::setup_config_manually (size_t io_blk_sz, size_t pg_sz, size_t nr_pgs) {
    // assert (((io_blk_sz & (io_blk_sz - 1)) == 0) && "'IO Block Size' must be a power of 2.");
    assert ((nr_pgs != 0) && "'No. of pages' must be != 0.");
    assert (((pg_sz % io_blk_sz) == 0) && "'IO Block Size' must be a multiple of 'Page Size'");

    this->CACHE_NR_PAGES  = nr_pgs;
    this->CACHE_PAGE_SIZE = pg_sz;
    this->IO_BLOCK_SIZE   = io_blk_sz;
}

void Config::setup_config_by_size (size_t prealloc_bytes, int nr_blocks_per_page) {

    this->is_default_config = false;
    assert (prealloc_bytes >= this->IO_BLOCK_SIZE);

    size_t total_bytes = std::ceil (prealloc_bytes);

    this->CACHE_PAGE_SIZE = nr_blocks_per_page * this->IO_BLOCK_SIZE;

    assert (((total_bytes > (nr_blocks_per_page * this->IO_BLOCK_SIZE))));

    this->CACHE_NR_PAGES = total_bytes / this->CACHE_PAGE_SIZE;
}

Config::~Config () {}

void Config::set_eviction_flag (bool flag) { this->APPLY_LRU_EVICTION = flag; }

void Config::print_config () {

    size_t total_bytes = this->CACHE_NR_PAGES * this->CACHE_PAGE_SIZE;

    spdlog::info ("[config] using a {} config", this->is_default_config ? "default" : "custom");
    spdlog::info ("[config] log all operations = {}, logfile = '{}'",
                  this->log_all_operations ? "true" : "false",
                  this->LOG_FILE == "" ? "false" : this->LOG_FILE);
    spdlog::info ("[config] no. of pages   = {}", this->CACHE_NR_PAGES);
    spdlog::info ("[config] page size      = {}", this->CACHE_PAGE_SIZE);
    spdlog::info ("[config] block size     = {}", this->IO_BLOCK_SIZE);
    spdlog::info ("[config] blocks / page  = {}", this->CACHE_PAGE_SIZE / this->IO_BLOCK_SIZE);
    spdlog::info ("[config] apply eviction = {}", this->APPLY_LRU_EVICTION ? "true" : "false");
    spdlog::info ("[config] total          = {} KiB, {} MiB, {} GiB",
                  (double)total_bytes / 1024,
                  (double)total_bytes / std::pow (1024, 2),
                  (double)total_bytes / std::pow (1024, 3));
    spdlog::info ("[config] snapshot: files regex = '{}', save = '{}'", this->SNAPSHOT_FILES, this->SNAPSHOT_SAVE);

}

} // namespace cache::config