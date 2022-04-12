#ifndef CACHE_ITEM_DATA_HPP
#define CACHE_ITEM_DATA_HPP

#include <cache/item/block_info.hpp>
#include <map>
#include <unordered_map>
#include <vector>

using namespace std;

namespace cache::item {

class Data {

  private:
    unordered_map<int, BlockInfo*> blocks;
    BlockInfo* _get_block_info_ptr (int blk_id);

  public:
    Data ();
    ~Data ();

    int get_page_id (int blk_id);
    int set_block_page_id (int blk_id, int allocated_page, int readable_from, int readable_to);
    pair<int, int> _get_readable_offsets (int blk_id);
    void remove_block (int blk_id);
    bool has_block (int blk_id);
    void print_data ();
    map<int, int> get_blocks_max_offsets ();
    size_t count_blocks ();
    void remove_all_blocks ();
    unordered_map<int, int> truncate_blocks_after (int blk_id, int blk_byte_index);
};

} // namespace cache::item

#endif // CACHE_ITEM_DATA_HPP