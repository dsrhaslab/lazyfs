#ifndef CACHE_ENGINE_PAGE_BLOCKS_HPP
#define CACHE_ENGINE_PAGE_BLOCKS_HPP

#include <map>
#include <unordered_map>

using namespace std;

namespace cache::engine::page {

/**
 * Maps the allocated blocks with their metadata information.
 */
class BlockOffsets {

  private:
    /**
     * @brief The offsets within the page where the block is readable.
     * For example, if a page of 8k has two 4k blocks, the first will
     * be readable from 0 to 4095 and the other from 4096 to 8191.
     *
     */
    unordered_map<int, pair<int, int>> block_offset_mapping;

    /**
     * @brief Sets the max readable data within the block offsets
     *
     */
    unordered_map<int, int> block_readable_to;

  public:
    /**
     * @brief Pretty print the Block Offsets object
     *
     */
    void _print_blockoffsets ();

    /**
     * @brief Resets the page data structures, marking it as clean.
     *
     */
    void reset ();

    /**
     * @brief Checks if a given block is present in the page.
     *
     * @param block_id the block id
     * @return true block exists
     * @return false block does not exist
     */
    bool contains_block (int block_id);

    /**
     * Gets the block offsets within the page
     */
    pair<int, int> get_block_offsets (int block_id);

    /**
     * @brief Get the number of blocks allocated in the page
     *
     * @return int number of blocks
     */
    int get_nr_blocks ();

    /**
     * @brief updates the block offsets to the allocated block or inserts
     * if not exists.
     *
     * @param block_id the block id
     * @param offsets block offsets
     */
    void insert_or_update_block_offsets (int block_id, pair<int, int> offsets);

    /**
     * @brief Sets the max readable offset within the block
     *
     * @param blk_id the block id
     * @param max_offset the max readable index
     */
    void make_readable_to (int blk_id, int max_offset);

    /**
     * @brief Get the max readable index for each block
     *
     * @return map<int, int> each block mapped to the max index
     */
    map<int, int> get_block_readable_offsets ();

    /**
     * @brief Preallocates block offsets structures
     *
     */
    void preallocate ();

    /**
     * @brief Get the max readable index for a block
     *
     * @param block_id the block id
     * @return int max index
     */
    int get_readable_to (int block_id);

    /**
     * @brief Deallocates a block
     *
     * @param blk_id the block id
     */
    void remove_block (int blk_id);

    /**
     * @brief Checks if no blocks are allocated.
     *
     * @return true no blocks are allocated
     * @return false there are blocks allocated
     */
    bool empty ();
};

} // namespace cache::engine::page

#endif // CACHE_ENGINE_PAGE_BLOCKS_HPP