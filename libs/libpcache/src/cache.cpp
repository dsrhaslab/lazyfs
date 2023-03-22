
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
#include <spdlog/spdlog.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <thread>
#include <tuple>
#include <utility>

using namespace std;
using namespace cache::engine;

namespace cache {

Cache::Cache (cache::config::Config* cache_config, PageCacheEngine* choosenEngine) {

    cache_config->print_config ();
    this->engine       = choosenEngine;
    this->cache_config = cache_config;
    this->contents.reserve (1000);
    this->file_inode_mapping.reserve (1000);
}

Cache::~Cache () {

    for (auto const& it : this->contents) {
        delete it.second;
    }
}

double Cache::get_cache_usage () { return this->engine->get_engine_usage (); }

void Cache::print_cache () {

    cout << "---------------------------- CACHE ----------------------------" << endl;

    cout << "=> contents:" << endl;

    for (auto const& it : this->contents) {
        cout << "<" << it.first << ">:\n";
        it.second->print_item ();
    }

    cout << "=> inode-mapping:" << endl;

    cout << "---------------------------------------------------------------" << endl;

    for (auto const& it : this->file_inode_mapping) {
        cout << "(file) " << it.first << " => (inode) " << it.second << endl;
    }

    cout << "---------------------------------------------------------------" << endl;
}

void Cache::print_engine () { this->engine->print_page_cache_engine (); }

string Cache::get_original_inode (string path) {
    lock_guard<std::shared_mutex> lock (lock_cache_mtx);
    if (this->file_inode_mapping.find (path) != this->file_inode_mapping.end ()) {
        return this->file_inode_mapping.at (path);
    }
    return "";
}

void Cache::insert_inode_mapping (string path, string inode, bool increase) {

    lock_guard<std::shared_mutex> lock (lock_cache_mtx);
    this->file_inode_mapping[path] = inode;

    if (increase) {
        Metadata* oldmeta = get_content_metadata (inode);
        Metadata newmeta;
        if (oldmeta != nullptr) {
            newmeta.nlinks = oldmeta->nlinks + 1;
            this->update_content_metadata (inode, newmeta, {"nlinks"});
        }
    }
}

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

    std::lock_guard<shared_mutex> lock (lock_cache_mtx);

    bool is_cached = false;

    if (has_content_cached (cid)) {

        int page  = this->contents.at (cid)->get_data ()->get_page_id (blk_id);
        is_cached = this->engine->is_block_cached (cid, page, blk_id);
    }

    return is_cached;
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

    this->item_locks.insert (make_pair (cid, new std::shared_mutex ()));
    this->contents.insert (make_pair (cid, cid_item));

    return cid_item;
}

void Cache::lockItem (string cid) { item_locks[cid]->lock (); }

void Cache::unlockItem (string cid) { item_locks[cid]->unlock (); }

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

    std::unique_lock<shared_mutex> lock (lock_cache_mtx, std::defer_lock);
    lock.lock ();

    Item* item = _get_content_ptr (cid);

    bool is_new_item = item == nullptr;
    if (is_new_item)
        item = _create_item (cid);

    lockItem (cid);
    lock.unlock ();

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

    bool allocated_at_least_one_page = false;

    for (auto const& it : allocations) {

        int bid            = it.first;
        int allocated_page = it.second;

        tuple<const char*, size_t, int, int> block_offsets = blocks.at (bid);

        // int rs_from = get<2> (block_offsets);
        int rs_to = get<3> (block_offsets);

        if (allocated_page >= 0) {
            allocated_at_least_one_page = true;
            // todo: recheck rs_from = 0?
            // item->get_data ()->set_block_page_id (bid, allocated_page, rs_from, rs_to);
            int max_offset = item->get_data ()->set_block_page_id (bid, allocated_page, 0, rs_to);
            this->engine->make_block_readable_to_offset (cid, allocated_page, bid, max_offset);
        } else
            item->get_data ()->remove_block (bid);

        put_res.insert (make_pair (bid, allocated_page >= 0 ? true : false));
    }

    if (allocated_at_least_one_page) {
        item->set_data_sync_flag (false);
    }

    unlockItem (cid);

    return put_res;
}

// returns: false if content was not found
// if block was not found, block_data == null
map<int, pair<bool, pair<int, int>>> Cache::get_data_blocks (string cid, map<int, char*> blocks) {

    std::unique_lock<shared_mutex> lock (lock_cache_mtx, std::defer_lock);
    lock.lock ();

    if (!has_content_cached (cid)) {
        lock.unlock ();
        return {};
    }

    lockItem (cid);
    lock.unlock ();

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

        this->contents.erase (cid);
    }

    return true;
}

bool Cache::truncate_item (string owner, off_t new_size) {

    std::unique_lock<shared_mutex> lock (lock_cache_mtx, std::defer_lock);
    lock.lock ();

    if (not has_content_cached (owner)) {
        lock.unlock ();
        return false;
    }

    lockItem (owner);
    lock.unlock ();

    Item* item = _get_content_ptr (owner);

    if (new_size == 0) {

        if (item->get_data ()->count_blocks () > 0) {
            this->engine->remove_cached_blocks (owner);
            item->get_data ()->remove_all_blocks ();
        }

        unlockItem (owner);
        return true;
    }

    int truncate_from_block_id      = new_size / this->cache_config->IO_BLOCK_SIZE;
    off_t truncate_from_block_index = new_size % this->cache_config->IO_BLOCK_SIZE;

    auto res = item->get_data ()->truncate_blocks_after (truncate_from_block_id,
                                                         truncate_from_block_index);

    this->engine->truncate_cached_blocks (owner,
                                          res,
                                          truncate_from_block_id,
                                          truncate_from_block_index);

    item->set_data_sync_flag (false);

    unlockItem (owner);

    return true;
}

int Cache::sync_owner (string owner, bool only_sync_data, char* orig_path) {

    std::unique_lock<shared_mutex> lock (lock_cache_mtx, std::defer_lock);
    lock.lock ();

    if (!has_content_cached (owner)) {

        lock.unlock ();
        return -1;
    }

    lockItem (owner);
    lock.unlock ();

    int res;

    // sync data

    off_t last_size = get_content_metadata (owner)->size;

    res = this->engine->sync_pages (owner, last_size, orig_path);
    _get_content_ptr (owner)->set_data_sync_flag (true);

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

    bool return_val = true;

    std::lock_guard<shared_mutex> lock (lock_cache_mtx);

    if (this->file_inode_mapping.find (old_cid) != this->file_inode_mapping.end ()) {
        string inode = this->file_inode_mapping.at (old_cid);

        string to_remove_inode = "";

        if (this->file_inode_mapping.find (new_cid) != this->file_inode_mapping.end ()) {
            to_remove_inode = this->file_inode_mapping.at (new_cid);
        }

        this->file_inode_mapping.erase (old_cid);
        this->file_inode_mapping[new_cid] = inode;

        if (has_content_cached (to_remove_inode)) {

            Item* item = _get_content_ptr (to_remove_inode);

            if (item != nullptr) {

                lockItem (to_remove_inode);

                nlink_t before_nlinks = item->get_metadata ()->nlinks;

                Metadata new_meta_after_removal;
                new_meta_after_removal.nlinks = std::max ((int)before_nlinks - 1, 1);
                item->update_metadata (new_meta_after_removal, {"nlinks"});

                if (before_nlinks > 1) {

                    unlockItem (to_remove_inode);
                    return return_val;
                }

                this->engine->remove_cached_blocks (to_remove_inode);
                this->_delete_item (to_remove_inode);

                unlockItem (to_remove_inode);

                if (!this->item_locks.empty () &&
                    (this->item_locks.find (to_remove_inode) != this->item_locks.end ())) {
                    this->item_locks.erase (to_remove_inode);
                }
            }
        }
    }

    return return_val;
}

bool Cache::remove_cached_item (string owner, const char* path, bool is_from_cache) {

    std::unique_lock<shared_mutex> lock (lock_cache_mtx, std::defer_lock);
    lock.lock ();

    if (not has_content_cached (owner)) {

        lock.unlock ();
        return false;
    }

    lockItem (owner);

    this->file_inode_mapping.erase (string (path));

    Item* item = _get_content_ptr (owner);

    nlink_t before_nlinks = item->get_metadata ()->nlinks;

    Metadata new_meta_after_removal;
    new_meta_after_removal.nlinks = std::max ((int)before_nlinks - 1, 1);
    item->update_metadata (new_meta_after_removal, {"nlinks"});

    if (!is_from_cache && before_nlinks > 1) {

        unlockItem (owner);
        lock.unlock ();
        return false;
    }

    this->engine->remove_cached_blocks (owner);
    this->_delete_item (owner);

    unlockItem (owner);

    if (!this->item_locks.empty () && (this->item_locks.find (owner) != this->item_locks.end ())) {
        this->item_locks.erase (owner);
    }

    lock.unlock ();

    return true;
}

void Cache::clear_all_cache () {

    std::list<pair<string, string>> items;
    std::for_each (this->file_inode_mapping.begin (),
                   this->file_inode_mapping.end (),
                   [&] (const std::pair<const string, string>& ref) {
                       items.push_back (make_pair (ref.first, ref.second));
                   });

    for (auto const& it : items) {

        remove_cached_item (it.second, (char*)it.first.c_str (), true);
    }

    {
        std::unique_lock<shared_mutex> lock (lock_cache_mtx, std::defer_lock);
        lock.lock ();

        std::list<string> item_buckets;
        std::for_each (this->contents.begin (),
                       this->contents.end (),
                       [&] (const std::pair<const string, Item*>& ref) {
                           item_buckets.push_back (ref.first);
                       });

        for (auto const& it : item_buckets) {

            lockItem (it);

            this->engine->remove_cached_blocks (it);

            this->_delete_item (it);

            unlockItem (it);

            if (!this->item_locks.empty () &&
                (this->item_locks.find (it) != this->item_locks.end ())) {
                this->item_locks.erase (it);
            }
        }

        lock.unlock ();
    }
}

bool Cache::lockItemCheckExists (string cid) {

    std::lock_guard<shared_mutex> lock (lock_cache_mtx);

    if (not has_content_cached (cid)) {
        return false;
    }

    try {

        lockItem (cid);

        return true;

    } catch (...) { return false; }
}

void Cache::full_checkpoint () {

    for (auto const& it : this->file_inode_mapping)
        this->sync_owner (it.second, false, (char*)it.first.c_str ());
}

std::vector<tuple<string, size_t, vector<tuple<int, pair<int, int>, int>>>>
Cache::report_unsynced_data () {

    std::lock_guard<shared_mutex> lock (lock_cache_mtx);

    std::vector<tuple<string, size_t, vector<tuple<int, pair<int, int>, int>>>> unsynced_data;

    for (auto const& it : this->contents) {
        if (not it.second->is_synced ()) {
            unsynced_data.push_back (
                std::make_tuple (it.first, 0, this->engine->get_dirty_blocks_info (it.first)));
        }
    }

    return unsynced_data;
}

vector<string> Cache::find_files_mapped_to_inode (string inode) {

    std::lock_guard<shared_mutex> lock (lock_cache_mtx);

    vector<string> res;

    for (auto it = this->file_inode_mapping.begin (); it != this->file_inode_mapping.end (); ++it) {
        if (it->second == inode)
            res.push_back (it->first);
    }

    return res;
}

} // namespace cache