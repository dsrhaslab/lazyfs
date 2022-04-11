#ifndef CACHE_ENGINE_PAGE_HPP
#define CACHE_ENGINE_PAGE_HPP

#include <array>
#include <cache/config/config.hpp>
#include <cache/constants/constants.hpp>
#include <cache/engine/page/block_offsets.hpp>
#include <map>
#include <string>
#include <vector>

using namespace std;

namespace cache::engine::page {

class Page {
  private:
    // true if page not flushed to disk
    bool is_dirty;
    // page data buffer (fixed to page size)

    string page_owner_id;

    vector<int> free_block_indexes;
    cache::config::Config* config;

    void _rewrite_offset_data (char* new_data, int off_min, int off_max);

  public:
    char* data;
    BlockOffsets allocated_block_ids;
    Page (cache::config::Config* config);
    ~Page ();
    void _print_page ();
    bool contains_block (int block_id);
    bool update_block_data (int block_id, char* new_data, size_t data_length, int off_start);
    pair<int, int> get_block_offsets (int block_id);
    pair<int, int> get_allocate_free_offset (int block_id);
    void get_block_data (int block_id, char* buffer, int read_to_max_index);
    void reset ();
    bool is_page_owner (string query);
    void change_owner (string new_owner);
    string get_page_owner ();
    bool has_free_space ();
    bool sync_data ();
    bool is_page_dirty ();
    void make_block_readable_to (int blk_id, int max_offset);
    void set_page_as_dirty ();
};

} // namespace cache::engine::page

#endif // CACHE_ENGINE_PAGE_HPP