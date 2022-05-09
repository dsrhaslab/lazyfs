
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
#include <sys/types.h>
#include <toml.hpp>

using namespace std;

namespace cache::config {

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

void Config::load_config (string filename) {

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

        if (filesystem_settings.contains ("sync_after_rename")) {
            bool rename_flag = toml::find (filesystem_settings, "sync_after_rename").as_boolean ();
            this->sync_after_rename = rename_flag;
        }
    }
}

void Config::set_eviction_flag (bool flag) { this->APPLY_LRU_EVICTION = flag; }

void Config::print_config () {

    size_t total_bytes = this->CACHE_NR_PAGES * this->CACHE_PAGE_SIZE;

    std::printf (
        "\t[config] Using the following (%s) configuration:\n\t\tNo. of pages = %lu\n\t\tPage "
        "size (bytes) = %lu\n\t\tBlock "
        "size (bytes) = %lu\n\t\tBlocks per page = %d\n\t\tApply eviction = %s\n\t\tSync after "
        "rename = %s\n\t\tTotal = "
        "%0.3f (KiB) or "
        "%0.3f (MiB) or "
        "%0.3f (GiB)\n",
        this->is_default_config ? "default" : "custom",
        (size_t)this->CACHE_NR_PAGES,
        (size_t)this->CACHE_PAGE_SIZE,
        (size_t)this->IO_BLOCK_SIZE,
        (int)((size_t)this->CACHE_PAGE_SIZE / (size_t)this->IO_BLOCK_SIZE),
        this->APPLY_LRU_EVICTION ? "true" : "false",
        this->sync_after_rename ? "true" : "false",
        (double)total_bytes / 1024,
        (double)total_bytes / std::pow (1024, 2),
        (double)total_bytes / std::pow (1024, 3));
}

} // namespace cache::config