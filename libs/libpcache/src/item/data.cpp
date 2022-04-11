
#include <cache/item/data.hpp>
#include <iostream>
#include <map>

using namespace std;

namespace cache::item {

Data::Data () { this->blocks.reserve (30000); }

Data::~Data () {
    for (auto const& it : this->blocks) {
        delete it.second;
    }
}

void Data::print_data () {
    for (auto const& it : this->blocks) {
        cout << "\t\tblk<" << it.first << "> (page=" << it.second->get_page_index_number () << ")";
        it.second->_print_block_info ();
    }
}

int Data::get_page_id (int blk_id) {

    if (!(this->blocks.find (blk_id) != this->blocks.end ()))
        return -1;

    return this->blocks.at (blk_id)->get_page_index_number ();
}

bool Data::has_block (int blk_id) { return (this->blocks.find (blk_id) != this->blocks.end ()); }

BlockInfo* Data::_get_block_info_ptr (int blk_id) {

    if (has_block (blk_id))
        return this->blocks.at (blk_id);

    return nullptr;
}

int Data::set_block_page_id (int blk_id, int allocated_page, int readable_from, int readable_to) {

    // unused for now
    (void)readable_from;

    BlockInfo* block_info = _get_block_info_ptr (blk_id);

    if (block_info == nullptr)
        block_info = new BlockInfo ();

    block_info->set_page_index_number (allocated_page);
    int max = block_info->make_readable_to (readable_to);

    this->blocks.insert (make_pair (blk_id, block_info));

    return max;
}

void Data::remove_block (int blk_id) {

    if (this->blocks.find (blk_id) != this->blocks.end ()) {

        delete this->blocks.at (blk_id);
        this->blocks.erase (blk_id);
    }
}

pair<int, int> Data::_get_readable_offsets (int blk_id) {
    return _get_block_info_ptr (blk_id)->clone_readable_offsets ();
}

map<int, int> Data::get_blocks_max_offsets () {

    map<int, int> res;

    for (auto it = this->blocks.begin (); it != this->blocks.end (); it++)
        res.insert (std::make_pair (it->first, _get_readable_offsets (it->first).second));

    return res;
}

} // namespace cache::item