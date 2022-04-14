#ifndef CACHE_HPP
#define CACHE_HPP

#include <cache/config/config.hpp>
#include <cache/engine/page_cache.hpp>
#include <cache/item/item.hpp>
#include <iostream>
#include <map>
#include <mutex>
#include <unordered_map>

using namespace std;

using namespace cache::item;
using namespace cache::engine;

namespace cache {

class Cache {

  private:
    cache::config::Config* cache_config;
    unordered_map<string, Item*> contents;
    unordered_map<string, mutex*> item_locks;
    PageCacheEngine* engine;

    Item* _get_content_ptr (string cid);
    pair<int, int> _get_readable_offsets (string cid, Item* item, int blk);

    mutex lock_cache_mtx;

  public:
    Cache (cache::config::Config* cache_config, PageCacheEngine* engine);
    ~Cache ();

    Item* _create_item (string cid);
    bool _delete_item (string cid);
    bool has_content_cached (string cid);
    bool update_content_metadata (string cid, Metadata new_meta, vector<string> values_to_update);
    Metadata* get_content_metadata (string cid);
    map<int, bool> put_data_blocks (string cid,
                                    map<int, tuple<const char*, size_t, int, int>> blocks,
                                    AllocateOperationType operation_type);
    // returns bid -> (was_found, null|(readable_from, readable_to))
    map<int, pair<bool, pair<int, int>>> get_data_blocks (string cid, map<int, char*> blocks);

    bool is_block_cached (string cid, int blk_id);
    void print_cache ();
    void print_engine ();
    void lockItem (string cid);
    void unlockItem (string cid);
    void lockCache ();
    void unlockCache ();
    double get_cache_usage ();
    bool remove_cached_item (string owner);
    int sync_owner (string owner, bool only_sync_data);
    bool rename_item (string old_cid, string new_cid);
    void clear_all_cache ();
    bool truncate_item (string owner, int new_size);
};

} // namespace cache

#endif // CACHE_HPP