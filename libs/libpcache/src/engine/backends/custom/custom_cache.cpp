
/**
 * @file custom_cache.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <algorithm>
#include <assert.h>
#include <cache/constants/constants.hpp>
#include <cache/engine/backends/custom/custom_cache.hpp>
#include <cache/util/utilities.hpp>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <map>
#include <sys/uio.h>
#include <tuple>
#include <unistd.h>
#include <unordered_map>

using namespace std;
using namespace cache::config;
using namespace cache::util;

namespace cache::engine::backends::custom {

CustomCacheEngine::CustomCacheEngine (cache::config::Config* config) {

    this->config = config;

    _print_with_time (
        "[engine] Pre-allocating " +
        to_string (((size_t) (this->config->CACHE_NR_PAGES * this->config->CACHE_PAGE_SIZE))) +
        " bytes...");

    // pre-allocate all cache pages
    int page_index = 0;
    this->search_index.reserve (this->config->CACHE_NR_PAGES);
    for (size_t it = 0; it < this->config->CACHE_NR_PAGES; it++, page_index++) {
        // allocate pages
        Page* p = new Page (config);

        // map page id to its address
        this->search_index.insert (pair<int, Page*> (page_index, p));
        // every page is free at start
        // or when data is flushed
        this->free_pages.push_back (page_index);
    }

    this->owner_free_pages_mapping.reserve (5);
    this->owner_pages_mapping.reserve (500);
    this->owner_ordered_pages_mapping.reserve (500);

    _print_with_time ("[engine] Pre-allocation finished");
}

CustomCacheEngine::~CustomCacheEngine () {

    for (auto const& it : this->search_index) {
        delete it.second;
    }
}

double CustomCacheEngine::get_engine_usage () {

    std::lock_guard<std::shared_mutex> lock (lock_cache_mtx);

    int used_pages = this->config->CACHE_NR_PAGES - this->free_pages.size ();
    double usage   = ((double)(used_pages * 1.0) / this->config->CACHE_NR_PAGES) * 100;

    return usage;
}

void CustomCacheEngine::print_page_cache_engine () {

    cout << "---------------------------- ENGINE ----------------------------" << endl;

    printf (":: Order (LRU): ");

    for (list<int>::iterator it = this->lru_main_vector.begin ();
         it != this->lru_main_vector.end ();
         it++) {

        if (it == prev (this->lru_main_vector.end (), 1))
            printf ("p(%d, lru) ", *it);
        else if (it == this->lru_main_vector.begin ())
            printf ("p(%d, head) -> ", *it);
        else
            printf ("p(%d) -> ", *it);
    }
    printf ("\n:: Cached pages: \n");
    for (auto it = this->search_index.begin (); it != this->search_index.end (); it++) {
        cout << "\t"
             << "-> p(" << it->first << ")";
        it->second->_print_page ();
        cout << endl;
    }
    printf (":: Owner <-> pages mapping: ");
    for (auto const& it : this->owner_pages_mapping) {
        cout << "\n\t-> '" << it.first << "' owns pages: | ";
        for (auto const& it2 : it.second) {
            cout << it2 << " | ";
        }
        cout << "\n";
    }
    cout << endl;

    cout << "----------------------------------------------------------------" << endl;
}

Page* CustomCacheEngine::_get_page_ptr (int page_id) {

    if (this->search_index.find (page_id) != this->search_index.end ())
        return this->search_index.at (page_id);

    return nullptr;
}

bool CustomCacheEngine::_has_empty_pages () { return this->free_pages.size () > 0; }

pair<int, Page*> CustomCacheEngine::_get_next_free_page (string owner_id) {

    // check if this owner has space left in his pages

    if (this->owner_pages_mapping.find (owner_id) != this->owner_pages_mapping.end ()) {

        auto free_pages = this->owner_free_pages_mapping.at (owner_id);

        if (free_pages.size () > 0) {

            int free_page = free_pages.back ();
            free_pages.pop_back ();

            return std::make_pair (free_page, _get_page_ptr (free_page));
        }
    }

    // otherwise, get an empty page

    if (CustomCacheEngine::_has_empty_pages ()) {

        int last_index = this->free_pages.back ();
        this->free_pages.pop_back ();
        return make_pair (last_index, _get_page_ptr (last_index));
    }

    // no empty pages

    if (this->config->APPLY_LRU_EVICTION) {

        int replace_page_id = this->lru_main_vector.back ();

        // cout << "[engine] _get_next_free_page: Applying eviction of page '" <<
        // replace_page_id
        //     << "'...\n";

        Page* page_to_reset = _get_page_ptr (replace_page_id);

        string old_owner = page_to_reset->get_page_owner ();

        auto blocks = page_to_reset->allocated_block_ids.get_block_readable_offsets ();

        if (this->owner_ordered_pages_mapping.find (old_owner) !=
            this->owner_ordered_pages_mapping.end ()) {

            for (auto const& it : blocks)
                this->owner_ordered_pages_mapping.at (old_owner).erase (it.first);
        }

        if (this->owner_pages_mapping.find (old_owner) != this->owner_pages_mapping.end ()) {
            this->owner_pages_mapping.at (old_owner).erase (replace_page_id);
            // todo: if in free pages remove from there
        }

        if (page_to_reset->is_page_dirty ())
            page_to_reset->sync_data ();

        // cout << "[engine] _get_next_free_page: Resetting page '" <<
        // replace_page_id << "'...\n";

        // this->owner_ordered_pages_mapping.at (owner);

        page_to_reset->reset ();

        return make_pair (replace_page_id, page_to_reset);
    }

    return make_pair (-1, nullptr);
}

void CustomCacheEngine::_apply_lru_after_page_visitation_on_WRITE (int visited_page_id) {

    lock_guard<mutex> lr (this->lru_management_mtx);

    // put visited page on the head of the lru buffer
    auto pos = this->page_order_mapping.find (visited_page_id);

    if (pos == this->page_order_mapping.end ()) {

        this->lru_main_vector.push_front (visited_page_id);
        this->page_order_mapping[visited_page_id] = this->lru_main_vector.begin ();

        if ((this->page_order_mapping.size ()) > this->config->CACHE_NR_PAGES) {

            this->page_order_mapping.erase (this->lru_main_vector.back ());
            this->lru_main_vector.pop_back ();
        }

    } else {

        this->lru_main_vector.erase (pos->second);
        this->lru_main_vector.push_front (visited_page_id);
        this->page_order_mapping[visited_page_id] = this->lru_main_vector.begin ();
    }
}

void CustomCacheEngine::_apply_lru_after_page_visitation_on_READ (int visited_page_id) {

    lock_guard<mutex> lr (this->lru_management_mtx);

    auto pos = this->page_order_mapping.find (visited_page_id);

    if (pos != this->page_order_mapping.end ())
        this->lru_main_vector.erase (pos->second);

    this->lru_main_vector.push_front (visited_page_id);
    this->page_order_mapping[visited_page_id] = this->lru_main_vector.begin ();
}

bool CustomCacheEngine::is_block_cached (string content_owner_id, int page_id, int block_id) {

    Page* p = _get_page_ptr (page_id);

    if (p == nullptr)
        return false;

    return p->is_page_owner (content_owner_id) && (*p).contains_block (block_id);
}

void CustomCacheEngine::_update_owner_pages (string new_owner,
                                             int page_id,
                                             int block_id,
                                             pair<int, int> block_offsets_inside_page) {

    Page* p = _get_page_ptr (page_id);
    if (p == nullptr)
        return;

    string real_owner = p->get_page_owner ();

    if (real_owner == "none")
        p->change_owner (new_owner);

    if (not(real_owner == new_owner)) {

        // change real owner
        p->change_owner (new_owner);

        // erase old owner page mapping

        if (this->owner_pages_mapping.find (real_owner) != this->owner_pages_mapping.end ()) {

            this->owner_pages_mapping.at (real_owner).erase (page_id);
            this->owner_ordered_pages_mapping.at (real_owner).erase (block_id);

            if (this->owner_pages_mapping.at (real_owner).empty ()) {

                this->owner_pages_mapping.erase (real_owner);
                this->owner_free_pages_mapping.erase (real_owner);
                this->owner_ordered_pages_mapping.erase (real_owner);
            }
        }
    }

    // update new_owner mapping

    if (this->owner_pages_mapping.find (new_owner) != this->owner_pages_mapping.end ()) {

        this->owner_pages_mapping.at (new_owner).insert (page_id);
        this->owner_ordered_pages_mapping.at (new_owner).insert (
            {block_id, {page_id, p, block_offsets_inside_page}});

    } else {

        unordered_set<int> owner_pages;
        owner_pages.reserve (30000);
        owner_pages.insert (page_id);

        this->owner_pages_mapping.insert (
            pair<string, unordered_set<int>> (new_owner, owner_pages));
        this->owner_free_pages_mapping.insert (pair<string, vector<int>> (new_owner, {}));
        this->owner_ordered_pages_mapping.insert (
            {new_owner, {{block_id, {page_id, p, block_offsets_inside_page}}}});
    }

    if (p->has_free_space ()) {
        this->owner_free_pages_mapping.at (new_owner).push_back (page_id);
    }
}

map<int, bool> CustomCacheEngine::get_blocks (string content_owner_id,
                                              map<int, tuple<int, char*, int>> block_pages) {

    map<int, bool> res_block_data;

    for (auto it = begin (block_pages); it != end (block_pages); it++) {

        std::shared_lock<std::shared_mutex> lock (lock_cache_mtx);

        int blk_id            = it->first;
        int page_id           = get<0> (it->second);
        char* data            = get<1> (it->second);
        int read_to_max_index = get<2> (it->second);

        Page* get_page = _get_page_ptr (page_id);

        if (get_page == nullptr || !get_page->is_page_owner (content_owner_id) ||
            !get_page->contains_block (blk_id)) {

            res_block_data.insert (pair<int, bool> (blk_id, false));

        } else {

            get_page->get_block_data (blk_id, data, read_to_max_index);
            res_block_data.insert (pair<int, bool> (blk_id, true));

            // todo: this if is not really needed, since this
            // _apply_lru_after_page_visitation_on_READ function does not remove from
            // the lru_main_vector
            if (this->config->APPLY_LRU_EVICTION)
                _apply_lru_after_page_visitation_on_READ (page_id);
        }
    }

    return res_block_data;
}

map<int, int> CustomCacheEngine::allocate_blocks (
    string content_owner_id,
    map<int, tuple<int, const char*, size_t, int>> block_data_mapping,
    AllocateOperationType operation_type) {

    // result blk_id -> page_idx
    map<int, int> res_block_allocated_pages;

    for (auto it = begin (block_data_mapping); it != end (block_data_mapping); it++) {

        std::unique_lock<std::shared_mutex> lock (lock_cache_mtx, std::defer_lock);

        lock.lock ();

        int blk_id          = it->first;
        int page_id         = get<0> (it->second);
        char* blk_data      = (char*)get<1> (it->second);
        size_t blk_data_len = get<2> (it->second);
        int offset_start    = get<3> (it->second);

        // client request block allocation that 'exists'...
        // check if that is true, otherwise find new page

        if (page_id >= 0) {

            Page* page_requested_ptr = _get_page_ptr (page_id);

            if ((page_requested_ptr != nullptr) &&
                page_requested_ptr->is_page_owner (content_owner_id) &&
                page_requested_ptr->contains_block (blk_id)) {

                // in this case we can update existing data
                page_requested_ptr->update_block_data (blk_id,
                                                       blk_data,
                                                       blk_data_len,
                                                       offset_start);

                res_block_allocated_pages.insert (pair<int, int> (blk_id, page_id));

                lock.unlock ();

                // skip page allocation algorithm
                continue;
            }
        }
        // default behaviour if a new free page is needed

        pair<int, Page*> next_free_page = _get_next_free_page (content_owner_id);

        int free_page_id    = next_free_page.first;
        page_id             = free_page_id;
        Page* free_page_ptr = next_free_page.second;

        if (page_id >= 0 && free_page_ptr != nullptr) {

            auto const& offs = free_page_ptr->get_allocate_free_offset (blk_id);
            free_page_ptr->update_block_data (blk_id, (char*)blk_data, blk_data_len, offset_start);

            if (operation_type == OP_WRITE)
                free_page_ptr->set_page_as_dirty ();

            // here, page_id = -1 indicates that the allocation failed
            res_block_allocated_pages.insert (pair<int, int> (blk_id, page_id));

            // apply replacement algorithm (LRU for example)
            if (this->config->APPLY_LRU_EVICTION)
                _apply_lru_after_page_visitation_on_WRITE (page_id);

            _update_owner_pages (content_owner_id, page_id, blk_id, offs);

        } else {

            // block allocation failed
            res_block_allocated_pages.insert (pair<int, int> (blk_id, -1));
        }

        lock.unlock ();
    }

    return res_block_allocated_pages;
}

bool CustomCacheEngine::remove_cached_blocks (string owner) {

    std::unique_lock<std::shared_mutex> lock (lock_cache_mtx);

    if (this->owner_free_pages_mapping.find (owner) != this->owner_free_pages_mapping.end ())
        this->owner_free_pages_mapping.erase (owner);

    if (this->owner_pages_mapping.find (owner) != this->owner_pages_mapping.end ()) {

        auto owner_pgs = this->owner_pages_mapping.at (owner);

        for (auto page_it = owner_pgs.begin (); page_it != owner_pgs.end (); page_it++) {

            // add page to free pages...
            this->free_pages.push_back (*page_it);

            if (this->config->APPLY_LRU_EVICTION) {

                auto pos = this->page_order_mapping.find (*page_it);
                if (pos != this->page_order_mapping.end ())
                    this->lru_main_vector.erase (pos->second);
                this->page_order_mapping.erase (*page_it);
            }

            Page* page_ptr = _get_page_ptr (*page_it);
            page_ptr->reset ();
            page_ptr->change_owner ("none");
        }

        this->owner_pages_mapping.erase (owner);
        this->owner_ordered_pages_mapping.erase (owner);
    }

    return true;
}

void CustomCacheEngine::make_block_readable_to_offset (string cid,
                                                       int page_id,
                                                       int block_id,
                                                       int offset) {

    std::unique_lock<std::shared_mutex> lock (lock_cache_mtx);

    Page* page_ptr = _get_page_ptr (page_id);
    if (page_ptr->is_page_owner (cid)) {
        page_ptr->make_block_readable_to (block_id, offset);
    }
}

bool CustomCacheEngine::sync_pages (string owner, off_t size, char* orig_path) {

    std::unique_lock<std::shared_mutex> lock (lock_cache_mtx);

    char const* path;
    path   = owner.c_str ();
    int fd = open (orig_path, O_WRONLY);

    // std::printf ("\tengine: (fsync) %s opened descriptor %d\n", owner.c_str (), fd);

    if (this->owner_pages_mapping.find (owner) != this->owner_pages_mapping.end ()) {

        off_t wrote_bytes = 0;
        off_t page_streak = 0;

        auto const& iterate_blocks = this->owner_ordered_pages_mapping.at (owner);
        off_t page_streak_last_offset =
            (iterate_blocks.begin ()->first) * this->config->IO_BLOCK_SIZE;

        vector<tuple<int, Page*, pair<int, int>>> page_chunk;
        page_chunk.reserve (iterate_blocks.size ());

        for (auto it = iterate_blocks.begin (); it != iterate_blocks.end (); it++) {

            auto current_block_id     = it->first;
            auto const& next_block_id = std::next (it, 1)->first;

            if ((page_streak < (__IOV_MAX - 1)) && (it != prev (iterate_blocks.end (), 1)) &&
                (current_block_id == (next_block_id - 1))) {

                page_streak++;

                page_chunk.push_back (it->second);

            } else {

                page_streak++;

                page_chunk.push_back (it->second);

                struct iovec iov[page_streak];

                page_streak_last_offset =
                    (current_block_id - page_streak + 1) * this->config->IO_BLOCK_SIZE;

                for (int p_id = 0; p_id < page_streak; p_id++) {

                    int streak_block = current_block_id - page_streak + p_id + 1;

                    auto const& streak_pair = page_chunk[p_id];
                    // auto const& streak_pair     = iterate_blocks.at (streak_block);
                    Page* page_ptr = get<1> (streak_pair);
                    // auto const& block_data_offs = page_ptr->get_block_offsets (streak_block);
                    auto const& block_data_offs = get<2> (streak_pair);
                    iov[p_id].iov_base          = page_ptr->data + block_data_offs.first;
                    if (p_id == page_streak - 1) {
                        iov[p_id].iov_len =
                            page_ptr->allocated_block_ids.get_readable_to (streak_block) + 1;
                    } else {
                        iov[p_id].iov_len = this->config->IO_BLOCK_SIZE;
                    }
                }

                wrote_bytes += pwritev (fd, iov, page_streak, page_streak_last_offset);

                page_streak = 0;

                page_chunk.clear ();

                page_streak_last_offset = (current_block_id + 1) * this->config->IO_BLOCK_SIZE;
            }
        }
    }

    ftruncate (fd, size);

    close (fd);

    return 0;
}

bool CustomCacheEngine::rename_owner_pages (string old_owner, string new_owner) {

    std::unique_lock<std::shared_mutex> lock (lock_cache_mtx);

    if (this->owner_pages_mapping.find (old_owner) == this->owner_pages_mapping.end ())
        return false;

    unordered_set<int> old_page_mapping = this->owner_pages_mapping.at (old_owner);
    vector<int> old_free_mapping        = this->owner_free_pages_mapping.at (old_owner);
    map<int, tuple<int, Page*, pair<int, int>>> old_ordered_pages =
        this->owner_ordered_pages_mapping.at (old_owner);

    for (auto const& it : old_page_mapping) {
        Page* page = _get_page_ptr (it);
        if (page)
            page->change_owner (new_owner);
    }

    this->owner_pages_mapping.erase (old_owner);
    this->owner_free_pages_mapping.erase (old_owner);
    this->owner_ordered_pages_mapping.erase (old_owner);

    this->owner_pages_mapping[new_owner]         = old_page_mapping;
    this->owner_free_pages_mapping[new_owner]    = old_free_mapping;
    this->owner_ordered_pages_mapping[new_owner] = old_ordered_pages;

    return true;
}

bool CustomCacheEngine::truncate_cached_blocks (string content_owner_id,
                                                unordered_map<int, int> blocks_to_remove,
                                                int from_block_id,
                                                off_t index_inside_block) {

    std::unique_lock<std::shared_mutex> lock (lock_cache_mtx);

    for (auto const& blk : blocks_to_remove) {

        int blk_id  = blk.first;
        int page_id = blk.second;

        Page* page = _get_page_ptr (page_id);

        if (page->is_page_owner (content_owner_id)) {

            // std::printf ("%s: %s is page %d owner\n", __func__, content_owner_id.c_str (),
            // page_id);

            if (blk_id == from_block_id && index_inside_block > 0) {

                if (page->contains_block (from_block_id)) {
                    page->make_block_readable_to (from_block_id, index_inside_block - 1);
                    page->write_null_from (from_block_id, index_inside_block);
                }

            } else {

                page->remove_block (blk_id);

                this->owner_pages_mapping.at (content_owner_id).erase (page_id);
                this->owner_ordered_pages_mapping.at (content_owner_id).erase (blk_id);

                if (not page->is_page_dirty ()) {

                    // add page to free pages...
                    this->free_pages.push_back (page_id);

                    if (this->config->APPLY_LRU_EVICTION) {

                        auto pos = this->page_order_mapping.find (page_id);
                        if (pos != this->page_order_mapping.end ())
                            this->lru_main_vector.erase (pos->second);
                        this->page_order_mapping.erase (page_id);
                    }

                    page->reset ();
                    page->change_owner ("none");
                }
            }
        }
    }

    return true;
}

} // namespace cache::engine::backends::custom
