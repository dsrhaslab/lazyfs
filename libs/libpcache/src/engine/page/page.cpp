
/**
 * @file page.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <assert.h>
#include <cache/constants/constants.hpp>
#include <cache/engine/page/page.hpp>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string.h>
#include <unistd.h>
#include <utility>

using namespace std;

namespace cache::engine::page {

Page::Page (cache::config::Config* config) {

    this->config        = config;
    this->is_dirty      = false;
    this->page_owner_id = "none";

    assert (this->config->CACHE_PAGE_SIZE % this->config->IO_BLOCK_SIZE == 0);

    this->data = (char*)malloc (this->config->CACHE_PAGE_SIZE);
    memset (this->data, 0, this->config->CACHE_PAGE_SIZE);
    this->free_block_indexes.reserve (this->config->CACHE_PAGE_SIZE / this->config->IO_BLOCK_SIZE);
    for (size_t i = 0; i < this->config->CACHE_PAGE_SIZE; i += this->config->IO_BLOCK_SIZE)
        this->free_block_indexes.push_back (i);

    this->allocated_block_ids.preallocate ();
}

Page::~Page () { free (this->data); }

bool Page::is_page_owner (string query) { return this->page_owner_id == query; }

void Page::change_owner (string new_owner) { this->page_owner_id = new_owner; }

string Page::get_page_owner () { return this->page_owner_id; }

bool Page::has_free_space () { return this->free_block_indexes.size () > 0; }

void Page::reset () {

    this->free_block_indexes.clear ();
    this->free_block_indexes.reserve (10);
    this->allocated_block_ids.reset ();
    this->is_dirty = false;
    // this->page_owner_id = "none";
    memset (this->data, 0, this->config->CACHE_PAGE_SIZE);
    for (size_t i = 0; i < this->config->CACHE_PAGE_SIZE; i += this->config->IO_BLOCK_SIZE)
        this->free_block_indexes.push_back (i);
}

void Page::_print_page () {

    cout << "<own=" << this->page_owner_id << ",dirty=" << this->is_dirty << ",free_idx("
         << this->free_block_indexes.size () << ")=[";
    for (auto it = this->free_block_indexes.begin (); it != this->free_block_indexes.end (); ++it) {
        if (it == prev (this->free_block_indexes.end (), 1))
            cout << *it << "-" << *it + this->config->IO_BLOCK_SIZE - 1;
        else
            cout << *it << "-" << *it + this->config->IO_BLOCK_SIZE - 1 << ",";
    }
    printf ("]\n");
    this->allocated_block_ids._print_blockoffsets ();
    cout << "\t-> data=(" << this->config->CACHE_PAGE_SIZE << " bytes) ";
    for (size_t i = 0; i < this->config->CACHE_PAGE_SIZE; i++) {
        if (i % this->config->IO_BLOCK_SIZE == 0)
            cout << "|" << this->data[i];
        else
            cout << this->data[i];
    }
    cout << "|" << endl;
}

bool Page::contains_block (int block_id) {

    return this->allocated_block_ids.contains_block (block_id);
}

pair<int, int> Page::get_allocate_free_offset (int block_id) {

    if (this->free_block_indexes.empty ())
        return make_pair (-1, -1);
    else {

        int free_index = this->free_block_indexes.back ();
        this->free_block_indexes.pop_back ();

        pair<int, int> allocated_offset =
            make_pair (free_index, free_index + this->config->IO_BLOCK_SIZE - 1);
        this->allocated_block_ids.insert_or_update_block_offsets (block_id, allocated_offset);

        return allocated_offset;
    }
}

void Page::get_block_data (int block_id, char* buffer, int read_to_max_index) {

    pair<int, int> offsets = get_block_offsets (block_id);
    int off_min            = offsets.first;
    int off_max            = read_to_max_index + 1;

    memcpy (buffer, this->data + off_min, off_max - off_min);
}

pair<int, int> Page::get_block_offsets (int block_id) {

    return this->allocated_block_ids.get_block_offsets (block_id);
}

void Page::_rewrite_offset_data (char* new_data, int off_min, int off_max) {

    memcpy (this->data + off_min, new_data, off_max - off_min + 1);
}

bool Page::update_block_data (int block_id, char* new_data, size_t data_length, int off_start) {

    pair<int, int> block_offsets = get_block_offsets (block_id);

    // block exists, update data
    int off_min = block_offsets.first;

    if (block_offsets.first >= 0 && block_offsets.second > 0) {

        assert (data_length <= this->config->IO_BLOCK_SIZE);

        this->_rewrite_offset_data ((char*)new_data,
                                    off_min + off_start,
                                    off_min + off_start + data_length - 1);

    } else
        return false;

    return true;
}

bool Page::is_page_dirty () { return this->is_dirty; }

void Page::set_page_as_dirty () { this->is_dirty = true; }

bool Page::sync_data () {

    char const* path;
    path   = this->page_owner_id.c_str ();
    int fd = open (path, O_WRONLY);

    int should_write = 0, actually_wrote = 0;

    map<int, int> block_rdble_offs = this->allocated_block_ids.get_block_readable_offsets ();

    for (auto it = block_rdble_offs.begin (); it != block_rdble_offs.end (); ++it) {
        if (contains_block (it->first)) {
            pair<int, int> data_offsets = get_block_offsets (it->first);
            int offset                  = it->first * this->config->IO_BLOCK_SIZE;
            int total_bytes             = this->config->IO_BLOCK_SIZE;
            if (it == prev (block_rdble_offs.end (), 1))
                total_bytes = it->second + 1;
            should_write += total_bytes;
            actually_wrote += pwrite (fd, this->data + data_offsets.first, total_bytes, offset);
        }
    }

    int res = should_write == actually_wrote;

    if (res)
        this->is_dirty = false;

    close (fd);

    return res;
}

void Page::make_block_readable_to (int blk_id, int max_offset) {

    this->allocated_block_ids.make_readable_to (blk_id, max_offset);
}

void Page::write_null_from (int block_id, int from_offset) {

    int off_first = get_block_offsets (block_id).first;

    // std::printf ("%s: from block %d, from offset %d (%d bytes)\n",
    //             __func__,
    //             block_id,
    //            off_first + from_offset,
    //             (int)this->config->IO_BLOCK_SIZE - off_first - from_offset);

    memset (this->data + off_first + from_offset,
            0,
            this->config->IO_BLOCK_SIZE - off_first - from_offset);
}

void Page::remove_block (int block_id) {
    if (contains_block (block_id)) {
        int off_first = get_block_offsets (block_id).first;
        this->free_block_indexes.push_back (off_first);
        this->allocated_block_ids.remove_block (block_id);

        memset (this->data + off_first, 0, this->config->IO_BLOCK_SIZE);
    }

    if (this->allocated_block_ids.empty ())
        this->is_dirty = false;
}

} // namespace cache::engine::page