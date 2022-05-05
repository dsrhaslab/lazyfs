
/**
 * @file cache.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <algorithm>
#include <cache/cache.hpp>
#include <cache/constants/constants.hpp>
#include <cache/item/item.hpp>
#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <thread>
#include <tuple>

using namespace std;
using namespace cache::engine;

namespace cache {

Cache::Cache (cache::config::Config* cache_config, PageCacheEngine* choosenEngine) {

    cache_config->print_config ();
    this->engine       = choosenEngine;
    this->cache_config = cache_config;
    this->contents.reserve (1000);
}

Cache::~Cache () {

    // delete this->engine;

    for (auto const& it : this->contents) {
        delete it.second;
    }

    for (auto const& it : this->item_locks) {
        delete it.second;
    }

    // delete this->cache_config;
}

double Cache::get_cache_usage () { return this->engine->get_engine_usage (); }

void Cache::print_cache () {

    cout << "---------------------------- CACHE ----------------------------" << endl;

    for (auto const& it : this->contents) {
        cout << "<" << it.first << ">:\n";
        it.second->print_item ();
    }

    cout << "---------------------------------------------------------------" << endl;
}

void Cache::print_engine () { this->engine->print_page_cache_engine (); }

bool Cache::has_content_cached (string cid) {

    if (not this->contents.empty ())
        return this->contents.find (cid) != this->contents.end ();
    return false;
}

Item* Cache::_get_content_ptr (string cid) {

    if (has_content_cached (cid))
        return this->contents.at (cid);

    return nullptr;
}

bool Cache::is_block_cached (string cid, int blk_id) {

    lockCache ();

    if (has_content_cached (cid)) {
        lockItem (cid);
        unlockCache ();
        int page = this->contents.at (cid)->get_data ()->get_page_id (blk_id);
        unlockItem (cid);
        return this->engine->is_block_cached (cid, page, blk_id);
    } else {
        unlockCache ();
    }

    return false;
}

// returns true if content was found and metadata updated
bool Cache::update_content_metadata (string cid,
                                     Metadata new_meta,
                                     vector<string> values_to_update) {

    Item* item = _get_content_ptr (cid);
    if (item == nullptr)
        return false;

    // content exists, update metadata

    item->update_metadata (new_meta, values_to_update);

    return true;
}

Metadata* Cache::get_content_metadata (string cid) {

    Item* item = _get_content_ptr (cid);
    if (item == nullptr)
        return nullptr;

    return item->get_metadata ();
}

Item* Cache::_create_item (string cid) {

    Item* cid_item = new Item ();

    this->item_locks.insert (make_pair (cid, new mutex ()));
    this->contents.insert (make_pair (cid, cid_item));

    return cid_item;
}

void Cache::lockCache () { lock_cache_mtx.lock (); }

void Cache::unlockCache () { lock_cache_mtx.unlock (); }

void Cache::lockItem (string cid) { this->item_locks.at (cid)->lock (); }

void Cache::unlockItem (string cid) { this->item_locks.at (cid)->unlock (); }

pair<int, int> Cache::_get_readable_offsets (string cid, Item* item, int blk_id) {

    auto const& data = item->get_data ();

    int page = data->get_page_id (blk_id);

    if (this->engine->is_block_cached (cid, page, blk_id)) {

        return item->get_data ()->_get_readable_offsets (blk_id);
    }

    return pair<int, int> (0, 0);
}

// returns: map<block_id, was_allocated>
map<int, bool> Cache::put_data_blocks (string cid,
                                       map<int, tuple<const char*, size_t, int, int>> blocks,
                                       AllocateOperationType operation_type) {

    map<int, tuple<int, const char*, size_t, int>> put_mapping;

    lockCache ();

    Item* item = _get_content_ptr (cid);

    bool is_new_item = item == nullptr;
    if (is_new_item)
        item = _create_item (cid);

    lockItem (cid);
    unlockCache ();

    for (auto const& it : blocks) {

        int bid           = it.first;
        const char* bdata = get<0> (it.second);
        size_t bdata_len  = get<1> (it.second);
        int off_start     = get<2> (it.second);

        int page_id =
            (is_new_item || (item == nullptr)) ? -1 : item->get_data ()->get_page_id (bid);

        put_mapping.insert (make_pair (bid, make_tuple (page_id, bdata, bdata_len, off_start)));
    }
    // request allocation to the engine
    map<int, int> allocations = this->engine->allocate_blocks (cid, put_mapping, operation_type);

    // return
    map<int, bool> put_res;

    for (auto const& it : allocations) {

        int bid            = it.first;
        int allocated_page = it.second;

        tuple<const char*, size_t, int, int> block_offsets = blocks.at (bid);

        // int rs_from = get<2> (block_offsets);
        int rs_to = get<3> (block_offsets);

        if (allocated_page >= 0) {
            // todo: recheck rs_from = 0?
            // item->get_data ()->set_block_page_id (bid, allocated_page, rs_from, rs_to);
            int max_offset = item->get_data ()->set_block_page_id (bid, allocated_page, 0, rs_to);
            this->engine->make_block_readable_to_offset (cid, allocated_page, bid, max_offset);
        } else
            item->get_data ()->remove_block (bid);

        put_res.insert (make_pair (bid, allocated_page >= 0 ? true : false));
    }

    unlockItem (cid);

    return put_res;
}

// returns: false if content was not found
// if block was not found, block_data == null
map<int, pair<bool, pair<int, int>>> Cache::get_data_blocks (string cid, map<int, char*> blocks) {

    lockCache ();

    if (!has_content_cached (cid)) {
        unlockCache ();
        return {};
    }

    lockItem (cid);
    unlockCache ();

    Item* item = _get_content_ptr (cid);

    map<int, tuple<int, char*, int>> get_mapping;

    for (auto const& it : blocks) {

        int blk_id           = it.first;
        char* store_data_ptr = it.second;
        int max_offset       = this->cache_config->IO_BLOCK_SIZE - 1;

        Data* item_data = item->get_data ();

        if (item_data->has_block (blk_id)) {

            int old_page = item_data->get_page_id (blk_id);
            get_mapping.insert (
                std::make_pair (blk_id, std::make_tuple (old_page, store_data_ptr, max_offset)));
        }
    }

    auto const& res = this->engine->get_blocks (cid, get_mapping);

    map<int, pair<bool, pair<int, int>>> cache_res;

    for (auto const& it : res) {
        if (it.second == false)
            item->get_data ()->remove_block (it.first);
        cache_res.insert (
            {it.first, std::make_pair (it.second, _get_readable_offsets (cid, item, it.first))});
    }

    unlockItem (cid);

    return cache_res;
}
bool Cache::_delete_item (string cid) {

    Item* item = _get_content_ptr (cid);
    if (item != nullptr) {
        delete item;
        if (!this->item_locks.empty () &&
            (this->item_locks.find (cid) != this->item_locks.end ())) {
            std::mutex* mtx = this->item_locks.at (cid);
            mtx->unlock ();
            this->item_locks.erase (cid);
            delete mtx;
        }
        this->contents.erase (cid);
    }

    return true;
}

bool Cache::truncate_item (string owner, int new_size) {

    lockCache ();

    if (not has_content_cached (owner)) {
        unlockCache ();
        return false;
    }

    lockItem (owner);
    unlockCache ();

    Item* item = _get_content_ptr (owner);

    if (new_size == 0) {

        if (item->get_data ()->count_blocks () > 0) {
            this->engine->remove_cached_blocks (owner);
            item->get_data ()->remove_all_blocks ();
        }

        unlockItem (owner);
        return true;
    }

    int truncate_from_block_id    = new_size / this->cache_config->IO_BLOCK_SIZE;
    int truncate_from_block_index = new_size % this->cache_config->IO_BLOCK_SIZE;

    auto res = item->get_data ()->truncate_blocks_after (truncate_from_block_id,
                                                         truncate_from_block_index);

    this->engine->truncate_cached_blocks (owner,
                                          res,
                                          truncate_from_block_id,
                                          truncate_from_block_index);

    unlockItem (owner);

    return true;
}

int Cache::sync_owner (string owner, bool only_sync_data) {

    if (!has_content_cached (owner))
        return -1;

    lockCache ();
    lockItem (owner);
    unlockCache ();

    int res;

    // sync data

    res = this->engine->sync_pages (owner);

    if (not only_sync_data) {

        // sync metadata

        Metadata* meta = _get_content_ptr (owner)->get_metadata ();

        struct timeval times[2];
        TIMESPEC_TO_TIMEVAL (&times[0], &(meta->atim));
        TIMESPEC_TO_TIMEVAL (&times[1], &(meta->mtim));

        const char* path = owner.c_str ();
        utimes (path, times);
    }

    unlockItem (owner);

    return res;
}

bool Cache::rename_item (string old_cid, string new_cid) {

    lockCache ();

    if (not has_content_cached (old_cid)) {
        unlockCache ();
        return false;
    }

    Item* old_item        = _get_content_ptr (old_cid);
    std::mutex* old_mutex = this->item_locks.at (old_cid);

    this->contents.erase (old_cid);
    this->item_locks.erase (old_cid);

    this->contents.insert (std::make_pair (new_cid, old_item));
    this->item_locks.insert (std::make_pair (new_cid, old_mutex));

    unlockCache ();

    return this->engine->rename_owner_pages (old_cid, new_cid);
}

bool Cache::remove_cached_item (string owner) {

    lockCache ();

    if (not has_content_cached (owner)) {
        unlockCache ();
        return false;
    }

    lockItem (owner);

    this->engine->remove_cached_blocks (owner);
    this->_delete_item (owner);

    unlockCache ();

    return true;
}

void Cache::clear_all_cache () {

    std::list<string> items;
    std::for_each (
        this->contents.begin (),
        this->contents.end (),
        [&] (const std::pair<const string, Item*>& ref) { items.push_back (ref.first); });

    for (auto const& it : items) {

        remove_cached_item (it);
    }
}

bool Cache::lockItemCheckExists (string cid) {

    lockCache ();

    if (not has_content_cached (cid)) {
        unlockCache ();
        return false;
    }

    lockItem (cid);

    unlockCache ();

    return true;
}

void Cache::full_checkpoint () {

    for (auto const& it : this->contents)
        this->sync_owner (it.first, false);
}

} // namespace cache