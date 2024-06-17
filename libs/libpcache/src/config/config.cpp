
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
#include <optional>

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

unordered_map<string,vector<faults::Fault*>> Config::load_config (string filename) {

    const auto data = toml::parse (filename);

    const auto& faults_settings = toml::find (data, "faults");

    if (faults_settings.contains ("fifo_path")) {
        const string fifo_path = toml::find<toml::string> (faults_settings, "fifo_path");
        if (fifo_path.length () > 0) 
            this->FIFO_PATH = fifo_path;
    }

    if (faults_settings.contains ("fifo_path_completed")) {
        const string fifo_path_completed = toml::find<toml::string> (faults_settings, "fifo_path_completed");
        if (fifo_path_completed.length () > 0)
            this->FIFO_PATH_COMPLETED = fifo_path_completed;
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

    unordered_map<string,vector<faults::Fault*>> faults; //(path,[Fault1,Fault2,...])
    if (data.contains ("injection")) {
        const auto& programmed_injections = toml::find<toml::array>(data,"injection");
	
        //Parse each injection
        for (const auto& injection : programmed_injections) {
            if (!injection.contains("type")) throw std::runtime_error("Key 'type' for some injection of is not defined in the configuration file.");
            string type = toml::find<string>(injection,"type");

            bool valid_fault = true;
            string error_msg{};

            if (type == TORN_SEQ) {
                //Checking the values of the parameters of the reordering fault 
                valid_fault = true;
                error_msg = "The following errors were found in the configuration file for a fault of type \"torn-seq\": \n";

                string file{};
                if (!injection.contains("file")) {
                    valid_fault = false;
                    error_msg += "\tKey 'file' for some injection of type \"torn-seq\" is not defined in the configuration file.\n";
                } else 
                    file = toml::find<string>(injection,"file");

                string op{};
                if (!injection.contains("op")) {
                    valid_fault = false;
                    error_msg += "\tKey 'op' for some injection of type \"torn-seq\" is not defined in the configuration file.\n";
                } else 
                    op = toml::find<string>(injection,"op");

                int occurrence = 0;
                if (!injection.contains("occurrence")) {
                    valid_fault = false;
                    error_msg += "\tKey 'occurrence' for some injection of type \"torn-seq\" is not defined in the configuration file.\n";
                } else 
                    occurrence = toml::find<int>(injection,"occurrence");

                vector<int> persist;
                if (!injection.contains("persist")) {
                    valid_fault = false;
                    error_msg += "\tKey 'persist' for some injection of type \"torn-seq\" is not defined in the configuration file.\n";  
                } else {
                    persist = toml::find<vector<int>>(injection,"persist");
                    //Sorting the persist vector is necessary for injecting this fault
                    sort(persist.begin(), persist.end());
                }

                faults::ReorderF * fault = NULL;
                vector<string> errors;
                if (valid_fault) {
                    fault = new faults::ReorderF(op,persist,occurrence);
                    vector<string> errors = fault->validate();
                }

                if (!valid_fault || errors.size() > 0) {
                    for (string error : errors) {
                        error_msg += "\t" + error + "\n";
                    }
                    spdlog::error(error_msg);
                    
                } else {
            
                    auto it = faults.find(file);

                    if (it == faults.end()) {
                        vector<faults::Fault*> v_faults;
                        v_faults.push_back(fault);
                        faults[file] = v_faults;
                    } else {
                        //At the moment, only one torn-seq fault per file is acceptable.
                        for (faults::Fault* f : it->second) {
                            faults::ReorderF* reorder_fault = dynamic_cast<faults::ReorderF*>(f);

                            if (reorder_fault && reorder_fault->op == op) {
                                valid_fault = false;
                                spdlog::error("It is only acceptable one torn-seq fault per type of operation for a given file.");
                            }
                        }
                        if (valid_fault) (it->second).push_back(fault);
                    }

                }

            } else if (type == TORN_OP) {
                //Checking the values of the parameters of the split write fault 

                valid_fault = true;
                error_msg = "The following errors were found in the configuration file for a fault of type \"torn-op\": \n";

                string file{};
                if (!injection.contains("file")) {
                    valid_fault = false;
                    error_msg += "\tKey 'file' for some injection of type \"torn-op\" is not defined in the configuration file.\n";
                } else 
                    file = toml::find<string>(injection,"file");

                int occurrence = 0;
                if (!injection.contains("occurrence")) {
                    valid_fault = false;
                    error_msg += "\tKey 'occurrence' for some injection of type \"torn-op\" is not defined in the configuration file.\n";
                } else
                    occurrence = toml::find<int>(injection,"occurrence");

                vector<int> persist;
                if (!injection.contains("persist")) {
                    valid_fault = false;
                    error_msg += "\tKey 'persist' for some injection of type \"torn-op\" is not defined in the configuration file.\n";
                } else
                    persist = toml::find<vector<int>>(injection,"persist");

                if (!injection.contains("parts") && !injection.contains("parts_bytes")) {
                    valid_fault = false;
                    error_msg += "\tNone of the keys 'parts' and 'key_parts' for some injection of type \"torn-op\" is defined in the configuration file. Please define at most one of them.\n";  
                }   

                if (injection.contains("parts") && injection.contains("parts_bytes")) {
                    valid_fault = false;
                    error_msg += "\tKeys 'parts' and 'key_parts' for some injection of type \"torn-op\" are exclusive in the configuration file. Please define at most one of them.\n";     
                }

                faults::SplitWriteF * fault = NULL;
                vector<string> errors;

                //A split write fault either contains the parameter "parts" or the "pats_bytes"
                if (valid_fault && injection.contains("parts")) {
                    int parts = toml::find<int>(injection,"parts");
                    
                    errors = faults::SplitWriteF::validate(occurrence,persist,parts,std::nullopt);

                    if (errors.size() <= 0) 
                        fault = new faults::SplitWriteF(occurrence,persist,parts);
                    else 
                        valid_fault = false;
                }

                if (valid_fault && injection.contains("parts_bytes")) {
                    vector<int> parts_bytes = toml::find<vector<int>>(injection,"parts_bytes");
                    
                    errors = faults::SplitWriteF::validate(occurrence,persist,std::nullopt,parts_bytes);

                    if (errors.size() <= 0) 
                        fault = new faults::SplitWriteF(occurrence,persist,parts_bytes);
                    else 
                        valid_fault = false;
                }

                if (!valid_fault) {
                    for (string error : errors) {
                        error_msg += "\t" + error + "\n";
                    }
                    spdlog::error(error_msg);

                } else {

                    auto it = faults.find(file);
                    if (it == faults.end()) {
                        vector<faults::Fault*> v_faults;
                        v_faults.push_back(fault);
                        faults[file] = v_faults;
                    } else {
                        //At the moment, only one split write fault per file is acceptable.
                        for (faults::Fault* f : it->second) {
                            faults::SplitWriteF* splitwrite_fault = dynamic_cast<faults::SplitWriteF*>(f);
                            if (splitwrite_fault) {
                                valid_fault = false;
                                spdlog::error("It is only acceptable one torn-op write fault per file.");
                            }
                        }
                        if (valid_fault) (it->second).push_back(fault);
                    }
                }

            } else if (type == CLEAR) {
                valid_fault = true;
                error_msg = "The following errors were found in the configuration file for a fault of type \"clear-cache\": \n";

                int occurrence = 0;
                if (!injection.contains("occurrence")) {
                    valid_fault = false;
                    error_msg += "\tKey 'occurrence' for some injection of type \"clear\" is not defined in the configuration file.\n";
                } else 
                    occurrence = toml::find<int>(injection,"occurrence");

                string timing{};
                if (!injection.contains("timing")) {
                    valid_fault = false;
                    error_msg += "\tKey 'timing' for some injection of type \"clear\" is not defined in the configuration file.\n";
                } else 
                    timing = toml::find<string>(injection,"timing");

                string op{};
                if (!injection.contains("op")) {
                    valid_fault = false;
                    error_msg += "\tKey 'op' for some injection of type \"clear\" is not defined in the configuration file.\n";
                } else 
                    op = toml::find<string>(injection,"op");

                string from = "none";
                if (injection.contains("from")) 
                    from = toml::find<string>(injection,"from");

                string to = "none";
                if (injection.contains("to")) 
                    to = toml::find<string>(injection,"to");

                bool crash = false;
                if (!injection.contains("crash")) {
                    valid_fault = false;
                    error_msg += "\tKey 'crash' for some injection of type \"clear\" is not defined in the configuration file.\n";
                } else 
                    crash = toml::find<bool>(injection,"crash");


                faults::ClearF * fault = NULL;
                vector<string> errors;
                if (valid_fault) {
                    fault = new faults::ClearF(timing,op,from,to,occurrence,crash);
                    errors = fault->validate();
                }

                if (!valid_fault || errors.size() > 0) {
                    for (string error : errors) {
                        error_msg +=  "\t" + error + "\n";
                    }
                    spdlog::error(error_msg);
                } else {

                    auto it = faults.find(from);
                    if (it == faults.end()) {
                        vector<faults::Fault*> v_faults;
                        v_faults.push_back(fault);
                        faults[from] = v_faults;
                    } else {
                        //At the moment, only one clear fault per file is acceptable.
                        for (faults::Fault* f : it->second) {
                            faults::ClearF* clear_fault = dynamic_cast<faults::ClearF*>(f);
                            if (clear_fault) {
                                valid_fault = false;
                                spdlog::error("It is only acceptable one clear fault per file.");
                            }
                        }
                        if (valid_fault) (it->second).push_back(fault);
                    }
                }
            
            } else {
                spdlog::error("Key 'type' for some injection has an unknown value in the configuration file.");
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