
#include <cache/engine/page/block_offsets.hpp>
#include <iostream>
#include <map>

namespace cache::engine::page {

void BlockOffsets::preallocate () {

    this->block_offset_mapping.reserve (10);
    this->block_readable_to.reserve (10);
}

bool BlockOffsets::contains_block (int block_id) {

    return this->block_offset_mapping.find (block_id) != this->block_offset_mapping.end ();
}

void BlockOffsets::reset () {
    this->block_offset_mapping.clear ();
    this->block_readable_to.clear ();
}

void BlockOffsets::_print_blockoffsets () {
    for (auto const& it : this->block_offset_mapping) {
        cout << "\t*blk(" << it.first << ") -> (from_byte: " << it.second.first
             << ", to_byte: " << it.second.second
             << ", max_offset: " << this->block_readable_to.at (it.first) << ")" << endl;
    }
}

pair<int, int> BlockOffsets::get_block_offsets (int block_id) {

    if (contains_block (block_id)) {

        return this->block_offset_mapping.at (block_id);
    }

    return std::make_pair (-1, -1);
}

int BlockOffsets::get_nr_blocks () { return this->block_offset_mapping.size (); }

void BlockOffsets::insert_or_update_block_offsets (int block_id, pair<int, int> offsets) {

    this->block_offset_mapping.insert (std::make_pair (block_id, offsets));
    this->block_readable_to.insert (std::make_pair (block_id, -1));
}

void BlockOffsets::make_readable_to (int blk_id, int max_offset) {

    this->block_readable_to[blk_id] = max_offset;
}

map<int, int> BlockOffsets::get_block_readable_offsets () {

    map<int, int> res;

    for (auto const& it : this->block_readable_to) {
        res.insert (std::make_pair (it.first, it.second));
    }

    return res;
}

int BlockOffsets::get_readable_to (int block_id) { return this->block_readable_to.at (block_id); }

void BlockOffsets::remove_block (int blk_id) {
    this->block_offset_mapping.erase (blk_id);
    this->block_readable_to.erase (blk_id);
}

bool BlockOffsets::empty () { return this->block_offset_mapping.empty (); }

} // namespace cache::engine::page
