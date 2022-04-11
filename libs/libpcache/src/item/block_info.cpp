
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

// Sorts and merges block intervals in N*Log(N) time
// void BlockInfo::add_merge_offsets (int from, int to) {

//     this->readable_offsets.push_back (make_pair (from, to));

//     sort (this->readable_offsets.begin (), this->readable_offsets.end (), compare);
//     vector<pair<int, int>> ans;
//     int i = 0;
//     int n = this->readable_offsets.size (), s = -1, e = -1;
//     while (i < n) {
//         if (s == -1) {
//             s = this->readable_offsets[i].first;
//             e = this->readable_offsets[i].second;
//             i++;
//         } else {
//             if (this->readable_offsets[i].first <= e) {
//                 e = max (e, this->readable_offsets[i].second);
//                 i++;
//             } else {
//                 ans.push_back (make_pair (s, e));
//                 s = this->readable_offsets[i].first;
//                 e = this->readable_offsets[i].second;
//                 i++;
//             }
//         }
//     }

//     if (s != -1) {
//         ans.push_back (make_pair (s, e));
//     }

//     this->readable_offsets = ans;
// }

int BlockInfo::get_page_index_number () { return this->page_index_number; }

void BlockInfo::_print_block_info () {

    cout << "\n\t\t\treadable from byte " << this->readable_offset.first << " - to "
         << this->readable_offset.second << endl;
}

} // namespace cache::item
