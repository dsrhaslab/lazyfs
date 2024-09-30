
/**
 * @file custom_cache.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef CACHE_ENGINE_CUSTOM_HPP
#define CACHE_ENGINE_CUSTOM_HPP

#include <cache/engine/page/page.hpp>
#include <cache/engine/page_cache.hpp>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace cache::engine::page;

namespace cache::engine::backends::custom {

/**
 * @brief Simple cache engine implementation with LRU management.
 *
 */
class CustomCacheEngine : public PageCacheEngine {

    /**
     * @brief Locks the page cache engine
     *
     */
    std::shared_mutex lock_cache_mtx;

  private:
    /**
     * @brief the LazyFS config object
     *
     */
    cache::config::Config* config;

    /**
     * @brief Cache eviction LRU for page ordering
     *
     */
    list<int> lru_main_vector;

    /**
     * @brief Page id mapped to the LRU iterator position
     *
     */
    unordered_map<int, list<int>::iterator> page_order_mapping;

    /**
     * @brief Page id mapped to its reference pointer for fast indexing
     *
     */
    unordered_map<int, Page*> search_index;

    /**
     * @brief A list of free page ids
     *
     */
    vector<int> free_pages;

    /**
     * @brief Maps content owners to the list of their page ids
     *
     */
    unordered_map<string, unordered_set<int>> owner_pages_mapping;

    /**
     * @brief Maps content associated blocks to the (page id, page ptr, offsets).
     * This avoids calling find, at, or similar multiple times by using redundancy.
     *
     */
    unordered_map<string, map<int, tuple<int, Page*, pair<int, int>, bool>>>
        owner_ordered_pages_mapping;

    /**
     * @brief Maps owners to their free page ids. A free page contains at least one
     * free block offset.
     *
     */
    unordered_map<string, vector<int>> owner_free_pages_mapping;

    /**
     * @brief Controls concurrency inside the LRU queue
     *
     */
    mutex lru_management_mtx;

    /**
     * @brief Gets a page reference
     *
     * @param page_id the page id
     * @return Page* the page reference
     */
    Page* _get_page_ptr (int page_id);

    /**
     * @brief Checks if the cache engine has free pages
     *
     * @return true at least one free page
     * @return false no free pages
     */
    bool _has_empty_pages ();

    /**
     * @brief Finds and allocates a free page. If no pages are free and
     * eviction is not configured, than no page will be allocated.
     *
     * @param owner_id the content id
     * @return pair<int, Page*> the page id or -1 mappend to the page reference or nullptr
     */
    pair<int, Page*> _get_next_free_page (string owner_id);

    /**
     * @brief Manages the LRU queue for a WRITE operation
     *
     * @param visited_page_id the page that was written
     */
    void _apply_lru_after_page_visitation_on_WRITE (int visited_page_id);

    /**
     * @brief Manages the LRU queue for a READ operation
     *
     * @param visited_page_id the page that was read
     */
    void _apply_lru_after_page_visitation_on_READ (int visited_page_id);

    /**
     * @brief Adds the block->page mapping to the content id associated data structures
     *
     * @param content_owner_id the content id
     * @param page_id the allocated page
     * @param block_id the allocated block
     * @param block_offsets_inside_page the allocated block offsets in that page
     */
    void _update_owner_pages (string content_owner_id,
                              int page_id,
                              int block_id,
                              pair<int, int> block_offsets_inside_page);

  public:
    /**
     * @brief Construct a new Custom Cache Engine object
     *
     * @param config the LazyFS configuration
     */
    CustomCacheEngine (cache::config::Config* config);

    /**
     * @brief Destroy the Custom Cache Engine object
     *
     */
    ~CustomCacheEngine ();

    map<int, int>
    allocate_blocks (string content_owner_id,
                     map<int, tuple<int, const char*, size_t, int>> block_data_mapping,
                     AllocateOperationType operation_type);

    map<int, bool> get_blocks (string content_owner_id,
                               map<int, tuple<int, char*, int>> block_pages);
    bool is_block_cached (string content_owner_id, int page_id, int block_id);
    void print_page_cache_engine ();
    double get_engine_usage ();
    bool remove_cached_blocks (string content_owner_id);
    bool sync_pages (string owner, off_t size, char* orig_path);
    bool partial_sync_pages (string owner, off_t size, char* orig_path, string parts);
    void make_block_readable_to_offset (string cid, int page_id, int block_id, int offset);
    bool rename_owner_pages (string old_owner, string new_owner);
    bool truncate_cached_blocks (string content_owner_id,
                                 unordered_map<int, int> blocks_to_remove,
                                 int from_block_id,
                                 off_t index_inside_block);
    vector<tuple<int, pair<int, int>, int>> get_dirty_blocks_info (string owner);
};

} // namespace cache::engine::backends::custom

#endif // CACHE_ENGINE_CUSTOM_HPP