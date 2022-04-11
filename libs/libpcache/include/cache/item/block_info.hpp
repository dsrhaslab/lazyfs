#ifndef CACHE_ITEM_BLOCK_INFO_H
#define CACHE_ITEM_BLOCK_INFO_H

#include <vector>

using namespace std;

namespace cache::item {

class BlockInfo {

  private:
    pair<int, int> readable_offset;
    int page_index_number;

  public:
    BlockInfo ();
    void set_page_index_number (int page_id);
    int get_page_index_number ();
    int make_readable_to (int to);
    void _print_block_info ();
    pair<int, int> clone_readable_offsets ();
};

} // namespace cache::item

#endif // CACHE_ITEM_BLOCK_INFO_H