
#include <algorithm>
#include <cache/item/block_info.hpp>
#include <iostream>

using namespace std;

namespace cache::item {

bool compare (const pair<int, int> it1, const pair<int, int> it2) {
    return (it1.first < it2.first);
}

BlockInfo::BlockInfo () {
    this->page_index_number      = -1;
    this->readable_offset.first  = 0;
    this->readable_offset.second = 0;
}

pair<int, int> BlockInfo::clone_readable_offsets () {
    pair<int, int> cloned_offsets (this->readable_offset);
    return cloned_offsets;
}

void BlockInfo::set_page_index_number (int page_id) { this->page_index_number = page_id; }

int BlockInfo::make_readable_to (int to) {

    if (to > this->readable_offset.second)
        this->readable_offset.second = to;

    return this->readable_offset.second;
}

int BlockInfo::get_page_index_number () { return this->page_index_number; }

void BlockInfo::_print_block_info () {

    cout << "\n\t\t\treadable from byte " << this->readable_offset.first << " - to "
         << this->readable_offset.second << endl;
}

void BlockInfo::truncate_readable_to (int to) { this->readable_offset.second = to; }

} // namespace cache::item
