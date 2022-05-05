
/**
 * @file data.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef CACHE_ITEM_DATA_HPP
#define CACHE_ITEM_DATA_HPP

#include <cache/item/block_info.hpp>
#include <map>
#include <unordered_map>
#include <vector>

using namespace std;

namespace cache::item {

/**
 * @brief Stores all blocks associated with a file in cache.
 *
 */

class Data {

  private:
    /**
     * @brief Maps a block index to the block info object.
     * Note: A block index is mapped to the real file block
     * offsets, e.g. block 2 corresponds to the offsets
     * 4096-8191.
     *
     */
    unordered_map<int, BlockInfo*> blocks;

    /**
     * @brief Returns the pointer to the block info, if block
     * id exists.
     *
     * @param blk_id the requested block id
     * @return BlockInfo* a non-null pointer to the block info, if it exists
     */
    BlockInfo* _get_block_info_ptr (int blk_id);

  public:
    /**
     * @brief Construct a new Data object
     *
     */
    Data ();

    /**
     * @brief Destroy the Data object
     *
     */
    ~Data ();

    /**
     * @brief Get the page id for a block id
     *
     * @param blk_id the requested block
     * @return int the page id or -1 if it does not exist
     */
    int get_page_id (int blk_id);

    /**
     * @brief Set the block page id
     *
     * @param blk_id Block id
     * @param allocated_page Page id
     * @param readable_from Low readable offset
     * @param readable_to High readable offset
     * @return int the block max readable index
     */
    int set_block_page_id (int blk_id, int allocated_page, int readable_from, int readable_to);

    /**
     * @brief Gets the block readable offsets
     *
     * @param blk_id Block id
     * @return pair<int, int> copy of readable offsets
     */
    pair<int, int> _get_readable_offsets (int blk_id);

    /**
     * @brief Removes the block id and its information
     *
     * @param blk_id Block id
     */
    void remove_block (int blk_id);

    /**
     * @brief Checks if a block id was stored.
     *
     * @param blk_id Block id
     * @return true Block exists
     * @return false Block does not exist
     */
    bool has_block (int blk_id);

    /**
     * @brief Pretty prints a Data object
     *
     */
    void print_data ();

    /**
     * @brief Get the blocks max readable offsets
     *
     * @return map<int, int> each block mapped to the max readable offset
     */
    map<int, int> get_blocks_max_offsets ();

    /**
     * @brief Counts the number of blocks
     *
     * @return size_t the number of blocks
     */
    size_t count_blocks ();

    /**
     * @brief Removes all blocks from the Data object.
     *
     */
    void remove_all_blocks ();

    /**
     * @brief Removes all blocks after the specified block, except if the
     * byte index is 0, which means that the block id should also be removed.
     *
     * @param blk_id Block id from which to truncate
     * @param blk_byte_index The index within the Block id
     * @return unordered_map<int, int> the block ids removed, mapped to its last page id
     */
    unordered_map<int, int> truncate_blocks_after (int blk_id, int blk_byte_index);
};

} // namespace cache::item

#endif // CACHE_ITEM_DATA_HPP