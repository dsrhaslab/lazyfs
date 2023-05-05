
/**
 * @file config.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef CACHE_CONFIG_HPP
#define CACHE_CONFIG_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <sys/types.h>
#include <atomic>

using namespace std;

namespace cache::config {

/**
 * @brief Stores the programmed faults present in the configuration file.
 */

class Fault {

  public:

    /**
     * @brief Operation related to the fault. 
     */
    string op;
    /**
     * @brief When op is called sequentially for a certain path, count the number of calls. When the sequence is broken, counter is set to 0.
     */
    std::atomic_int counter;
    /**
     * @brief If the vector is [3,4] it means that if op is called for a certain path sequentially, the 3th and 4th call will be persisted.
     */
    vector<int> persist;

    std::atomic_int count_ocurrence;
    int ocurrence;
  
    /**
     * @brief Construct a new Fault object.
     *
     * @param op operation (i.e. "write", ...)
     * @param persist vector with operations to persist
     */
    Fault(string op, vector<int> persist, int ocurrence);

    /**
     * @brief Default constructor for Fault.
     */    
    Fault();

    ~Fault ();
};

/**
 * @brief Stores the cache configuration parameters.
 *
 */

class Config {

  private:
    /**
     * @brief Sets up cache paraments by specifying the total size in bytes and the number
     * of blocks per page. By using this version, a 4k block size is considered.
     *
     * @param prealloc_bytes The total size in bytes.
     * @param nr_blocks_per_page Number of 4k blocks per page.
     */
    void setup_config_by_size (size_t prealloc_bytes, int nr_blocks_per_page);

    /**
     * @brief Sets up cache parameters manually, specifying each individual value.
     *
     * @param io_blk_sz
     * @param pg_sz
     * @param nr_pgs
     */
    void setup_config_manually (size_t io_blk_sz, size_t pg_sz, size_t nr_pgs);

  public:
    /**
     * @brief Wheter to log filesystem operations to stdout
     *
     */
    bool log_all_operations = false;
    /**
     * @brief Specifies if this configuration was specified by the configuration file or not.
     *
     */
    bool is_default_config = true;
    /**
     * @brief Specifies the number of pages in the cache.
     *
     */
    size_t CACHE_NR_PAGES = 5;
    /**
     * @brief Specifies the cache page size.
     *
     */
    size_t CACHE_PAGE_SIZE = 4096;
    /**
     * @brief Specifies the IO Block size.
     *
     */
    size_t IO_BLOCK_SIZE    = 4096;
    size_t DISK_SECTOR_SIZE = 512;
    /**
     * @brief Whether eviction should be applied in cache.
     *
     */
    int APPLY_LRU_EVICTION = false;
    /**
     * @brief The path for the faults fifo.
     *
     */
    string FIFO_PATH = "faults.fifo";
    /**
     * @brief The path for the LazyFS logfile.
     *
     */
    string LOG_FILE = "";

    /**
     * @brief Default constructor for Config.
     *
     */
    Config () {};

    /**
     * @brief Construct a new Config object manually.
     *
     * @param io_blk_sz IO Block size
     * @param pg_sz Cache: page size (must be a multiple of io_blk_sz)
     * @param nr_pgs Cache: number of pages
     */
    Config (size_t io_blk_sz, size_t pg_sz, size_t nr_pgs);

    /**
     * @brief Construct a new Config object
     *
     * @param prealloc_bytes
     * @param nr_blocks_per_page
     */
    Config (size_t prealloc_bytes, int nr_blocks_per_page);

    /**
     * @brief Destroy the Config object
     *
     */
    ~Config ();

    /**
     * @brief Sets the eviction flag
     *
     * @param flag True if eviction should be applied
     */
    void set_eviction_flag (bool flag);

    /**
     * @brief Pretty prints a Config object
     *
     */
    void print_config ();

    /**
     * @brief Loads and constructs a Config object from the LazyFS config file.
     *
     * @param filename Filename to read the config from
     * @return Map from files to programmed faults for those files
     */
    unordered_map<string,vector<Fault*>> load_config (string filename);
};

} // namespace cache::config

#endif