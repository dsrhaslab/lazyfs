#ifndef CACHE_ENGINE_CUSTOM_HPP
#define CACHE_ENGINE_CUSTOM_HPP

#include <cache/engine/page/page.hpp>
#include <cache/engine/page_cache.hpp>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace cache::engine::page;

namespace cache::engine::backends::custom {

class CustomCacheEngine : public PageCacheEngine {

    static pthread_rwlock_t lock_cache;

  private:
    cache::config::Config* config;
    // LRU ordered vector
    list<int> lru_main_vector;
    // maps page_index -> lru_main_vector_iterator_position
    unordered_map<int, list<int>::iterator> page_order_mapping;
    // page_index -> page_pointer
    unordered_map<int, Page*> search_index;
    // stores free page ids
    vector<int> free_pages;
    // owner -> [pages-list]
    unordered_map<string, unordered_set<int>> owner_pages_mapping;
    unordered_map<string, map<int, int>> owner_oredered_pages_mapping;
    unordered_map<string, vector<int>> owner_free_pages_mapping;
    // concurrency control
    mutex lru_management_mtx;

    Page* _get_page_ptr (int page_id);
    bool _has_empty_pages ();
    pair<int, Page*> _get_next_free_page (string owner_id);
    pair<Page*, pair<int, int>>* _get_free_page_offsets ();
    void _apply_lru_after_page_visitation_on_WRITE (int visited_page_id);
    void _apply_lru_after_page_visitation_on_READ (int visited_page_id);
    void _update_owner_pages (string content_owner_id, int page_id, int block_id);

  public:
    CustomCacheEngine (cache::config::Config* config);
    ~CustomCacheEngine ();
    map<int, int>
    // interface method
    allocate_blocks (string content_owner_id,
                     // block_id -> (allocated_page (or -1 if does not exist)), data, data_length)
                     map<int, tuple<int, const char*, size_t, int>> block_data_mapping,
                     AllocateOperationType operation_type);
    // (blk_id, <page_id, data_buffer>)
    map<int, bool> get_blocks (string content_owner_id,
                               map<int, tuple<int, char*, int>> block_pages);
    bool is_block_cached (string content_owner_id, int page_id, int block_id);
    void print_page_cache_engine ();
    double get_engine_usage ();
    bool remove_cached_blocks (string content_owner_id);
    bool sync_pages (string owner);
    void make_block_readable_to_offset (string cid, int page_id, int block_id, int offset);
    bool rename_owner_pages (string old_owner, string new_owner);
    bool truncate_cached_blocks (string content_owner_id,
                                 unordered_map<int, int> blocks_to_remove,
                                 int from_block_id,
                                 int index_inside_block);
};

} // namespace cache::engine::backends::custom

#endif // CACHE_ENGINE_CUSTOM_HPP