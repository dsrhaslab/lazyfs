#ifndef CACHE_ENGINE_PAGE_BLOCKS_HPP
#define CACHE_ENGINE_PAGE_BLOCKS_HPP

#include <map>
#include <unordered_map>

using namespace std;

namespace cache::engine::page {

class BlockOffsets {
  private:
    /**
     * @brief The offsets within the page where the block is readable.
     * For example, if a page of 8k has two 4k blocks, the first will
     * be readable from 0 to 4095 and the other from 4096 to 8191.
     *
     */
    // blk_id -> offsets inside the page, e.g. [0-4095] or [4096-8191]
    unordered_map<int, pair<int, int>> block_offset_mapping;
    // blk_id -> max offset readable, e.g. if offsets are [0-4095], the block
    // could be readable to 4000, from 4001-> 4095 is trash (zeros...)
    unordered_map<int, int> block_readable_to;

  public:
    void _print_blockoffsets ();
    void reset ();
    bool contains_block (int block_id);
    pair<int, int> get_block_offsets (int block_id);
    int get_nr_blocks ();
    void insert_or_update_block_offsets (int block_id, pair<int, int> offsets);
    void make_readable_to (int blk_id, int max_offset);
    map<int, int> get_block_readable_offsets ();
    void preallocate ();
    int get_readable_to (int block_id);
    void remove_block (int blk_id);
    bool empty ();
};

} // namespace cache::engine::page

#endif // CACHE_ENGINE_PAGE_BLOCKS_HPP