
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
#include <iostream>
#include <math.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <toml.hpp>
#include <atomic>
#include <typeinfo>

using namespace std;

namespace cache::config {

/*  Faults  */

Fault::Fault(string type) {
    this->type = type;
}

Fault::~Fault(){}

ReorderF::ReorderF(string op, vector<int> persist, int ocurrence) : Fault(REORDER) {
    (this->counter).store(0);
    this->op = op;
    this->persist = persist;
    this->ocurrence = ocurrence;
    (this->group_counter).store(0);

}

ReorderF::ReorderF() : Fault(REORDER) {
	vector <int> v;
	(this->counter).store(0);
    (this->group_counter).store(0);
    this->op = "";
	this->persist = v;
    this->ocurrence = 0;
}

ReorderF::~ReorderF(){}

SplitWriteF::SplitWriteF(int ocurrence, vector<int> persist, int parts) : Fault(SPLITWRITE) {
    (this->counter).store(0);
    this->ocurrence = ocurrence;
    this->persist = persist;
    this->parts = parts;
}

SplitWriteF::SplitWriteF(int ocurrence, vector<int> persist, vector<int> parts_bytes) : Fault(SPLITWRITE) {
    (this->counter).store(0);
    this->ocurrence = ocurrence;
    this->persist = persist;
    this->parts = -1;
    this->parts_bytes = parts_bytes;
}

SplitWriteF::SplitWriteF() : Fault(SPLITWRITE) {
	vector <int> v;
    vector<int> p;
	(this->counter).store(0);
    this->ocurrence = 0;
	this->persist = p;
    this->parts_bytes = v;
    this->parts = 0;
}

SplitWriteF::~SplitWriteF(){}

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

unordered_map<string,vector<Fault*>> Config::load_config (string filename) {

    const auto data = toml::parse (filename);

    const auto& faults_settings = toml::find (data, "faults");

    if (faults_settings.contains ("fifo_path")) {
        const auto fifo_path = toml::find (faults_settings, "fifo_path");
        string fifo_path_str = fifo_path.as_string ().str;
        if (fifo_path_str.length () > 0)
            this->FIFO_PATH = fifo_path_str;
    }

    const auto& cache_settings = toml::find (data, "cache");

    if (cache_settings.contains ("simple")) {

        const auto& cache_simple_settings = toml::find (data, "cache", "simple");

        const auto cache_size = toml::find (cache_simple_settings, "custom_size");
        string cache_size_str = cache_size.as_string ().str;

        string size_tmp;
        string size_type;

        for (int i = 0; i < (int)cache_size_str.length (); i++) {
            char curr_char = cache_size_str[i];
            if (isdigit (curr_char) || curr_char == '.')
                size_tmp += curr_char;
            else
                size_type += curr_char;
        }

        size_t size_bytes;

        std::for_each (size_type.begin (), size_type.end (), [] (char& c) { c = ::tolower (c); });

        if (size_type == "gb" || size_type == "g")
            size_bytes = One_Gigabyte;
        if (size_type == "mb" || size_type == "m")
            size_bytes = One_Megabyte;
        if (size_type == "kb" || size_type == "k")
            size_bytes = One_Kilobyte;

        size_t total = size_bytes * atof (size_tmp.c_str ());

        int bpp = 1;
        if (cache_simple_settings.contains ("blocks_per_page"))
            bpp = toml::find (cache_simple_settings, "blocks_per_page").as_integer ();

        this->setup_config_by_size (total, bpp);

    } else if (cache_settings.contains ("manual")) {

        const auto& cache_manual_settings = toml::find (data, "cache", "manual");

        const auto block_size = toml::find (cache_manual_settings, "io_block_size");
        const auto page_size  = toml::find (cache_manual_settings, "page_size");
        const auto no_pages   = toml::find (cache_manual_settings, "no_pages");

        this->setup_config_manually ((size_t)block_size.as_integer (),
                                     (size_t)page_size.as_integer (),
                                     (size_t)no_pages.as_integer ());
    }

    if (cache_settings.contains ("apply_eviction")) {
        bool eviction_flag = toml::find (cache_settings, "apply_eviction").as_boolean ();
        this->set_eviction_flag (eviction_flag);
    }

    if (data.contains ("filesystem")) {

        const auto& filesystem_settings = toml::find (data, "filesystem");

        if (filesystem_settings.contains ("log_all_operations")) {
            bool log_all_ops = toml::find (filesystem_settings, "log_all_operations").as_boolean ();
            this->log_all_operations = log_all_ops;
        }

        if (filesystem_settings.contains ("logfile")) {
            string logfile = toml::find (filesystem_settings, "logfile").as_string ();
            this->LOG_FILE = logfile;
        }
    }

    unordered_map<string,vector<Fault*>> faults; //(path,[Fault1,Fault2,...])
    if (data.contains ("injection")) {
        const auto& programmed_injections = toml::find<toml::array>(data,"injection");
	
        for (const auto& injection : programmed_injections) {
            if (!injection.contains("type")) throw std::runtime_error("Key 'type' for some injection of is not defined in the configuration file.");
            string type = toml::find<string>(injection,"type");

            if (type == REORDER) {

                if (!injection.contains("file")) throw std::runtime_error("Key 'file' for some injection of type \"reorder\" is not defined in the configuration file.");
                string file = toml::find<string>(injection,"file");

                if (!injection.contains("op")) throw std::runtime_error("Key 'op' for some injection of type \"reorder\" is not defined in the configuration file.");
                string op = toml::find<string>(injection,"op");

                if (!injection.contains("ocurrence")) throw std::runtime_error("Key 'ocurrence' for some injection of type \"reorder\" is not defined in the configuration file.");
                int ocurrence = toml::find<int>(injection,"ocurrence");
                if (ocurrence <= 0) throw std::runtime_error("Key 'ocurrence' for some injection of type \"split_write\" has an invalid value in the configuration file. It should be greater than 0.");

                if (!injection.contains("persist") && op == "write") throw std::runtime_error("Key 'persist' for some injection of type \"reorder\" with \"write\" as op is not defined in the configuration file.");     
                vector<int> persist = toml::find<vector<int>>(injection,"persist");
                sort(persist.begin(), persist.end()); 
                for (auto & p : persist) {
                    if (p <= 0) throw std::runtime_error("Key 'persist' for some injection of type \"reorder\" has an invalid value in the configuration file. The array must contain values greater than 0.");
                } 
            
                cache::config::ReorderF * fault = new ReorderF(op,persist,ocurrence);
                auto it = faults.find(file);

                if (it == faults.end()) {
                    vector<Fault*> v_faults;
                    v_faults.push_back(fault);
                    faults[file] = v_faults;
                } else {
                    for (Fault* fault : it->second) {
                        ReorderF* reorder_fault = dynamic_cast<ReorderF*>(fault);
                        if (reorder_fault && reorder_fault->op == op) throw std::runtime_error("It is only acceptable one reorder fault per type of operation for a given file.");
                    }
                    (it->second).push_back(fault);
                }
            } else if (type == SPLITWRITE) {

                if (!injection.contains("file")) throw std::runtime_error("Key 'file' for some injection of type \"split_write\" is not defined in the configuration file.");
                string file = toml::find<string>(injection,"file");

                if (!injection.contains("ocurrence")) throw std::runtime_error("Key 'ocurrence' for some injection of type \"split_write\" is not defined in the configuration file.");
                int ocurrence = toml::find<int>(injection,"ocurrence");
                if (ocurrence <= 0) throw std::runtime_error("Key 'ocurrence' for some injection of type \"split_write\" has an invalid value in the configuration file. It should be greater than 0.");

                if (!injection.contains("persist")) throw std::runtime_error("Key 'persist' for some injection of type \"split_write\" is not defined in the configuration file.");
                vector<int> persist = toml::find<vector<int>>(injection,"persist");

                if (!injection.contains("parts") && !injection.contains("parts_bytes")) throw std::runtime_error("None of the keys 'parts' and 'key_parts' for some injection of type \"split_write\" is defined in the configuration file. Please define at most one of them.");     

                if (injection.contains("parts") && injection.contains("parts_bytes")) throw std::runtime_error("Keys 'parts' and 'key_parts' for some injection of type \"split_write\" are exclusive in the configuration file. Please define at most one of them.");     

                cache::config::SplitWriteF * fault = NULL;
                if (injection.contains("parts")) {
                    int parts = toml::find<int>(injection,"parts");
                    
                    if (parts <= 0) throw std::runtime_error("Key 'parts' for some injection of type \"split_write\" has an invalid value in the configuration file. It should be greater than 0.");

                    for (auto & p : persist) {
                        if (p <= 0 || p>parts) throw std::runtime_error("Key 'persist' for some injection of type \"split_write\" has an invalid value in the configuration file. The array must contain values greater than 0 and lesser than the number of parts.");
                    } 
                    fault = new SplitWriteF(ocurrence,persist,parts);
                }

                if (injection.contains("parts_bytes")) {
                    vector<int> parts_bytes = toml::find<vector<int>>(injection,"parts_bytes");
                    
                    for (auto & part : parts_bytes) {
                        if (part <= 0) throw std::runtime_error("Key 'parts_bytes' for some injection of type \"split_write\" has an invalid value in the configuration file. Every part should be greater than 0.");
                    } 
                    for (auto & p : persist) {
                        if (p <= 0 || (size_t)p > parts_bytes.size()) throw std::runtime_error("Key 'persist' for some injection of type \"split_write\" has an invalid value in the configuration file. The array must contain values greater than 0 and lesser than the number of parts.");
                    }
                    fault = new SplitWriteF(ocurrence,persist,parts_bytes);
                }

                auto it = faults.find(file);
                if (it == faults.end()) {
                    vector<Fault*> v_faults;
                    v_faults.push_back(fault);
                    faults[file] = v_faults;
                } else {
                    (it->second).push_back(fault);
                }
                
            } else {
                throw std::runtime_error("Key 'type' for some injection has an unknown value in the configuration file.");
            }
        }
	
    }

    return faults;
}

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
}

} // namespace cache::config