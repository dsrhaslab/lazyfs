#ifndef CACHE_ITEM_BLOCK_INFO_H
#define CACHE_ITEM_BLOCK_INFO_H

#include <vector>

using namespace std;

namespace cache::item {

/**
 * @brief Stores metadata information about a block in the cache.
 *
 */

class BlockInfo {

  private:
    /**
     * @brief A block contains data from the file and can contain padding null bytes.
     * Therefore, for each block, we have the from/to byte index where it is readable.
     *
     */
    pair<int, int> readable_offset;
    /**
     * @brief The page where the block was last stored in Cache.
     * Note: The block could have been evicted if eviction is on, therefore the page
     * may not be owned by him anymore.
     *
     */
    int page_index_number;

  public:
    /**
     * @brief Construct a new Block Info object
     *
     */
    BlockInfo ();

    /**
     * @brief Sets the page index number for this block.
     *
     * @param page_id The page where the block is stored.
     */
    void set_page_index_number (int page_id);

    /**
     * @brief Get the page index number for this block.
     * This page can be used for later referencing.
     *
     * @return int The page number.
     */
    int get_page_index_number ();

    /**
     * @brief Sets the max index where the block bytes are readable.
     * If the index specified is less than the existing one, the value
     * remains the same.
     *
     * @param to the max index
     * @return int the max index after applying the method
     */
    int make_readable_to (int to);

    /**
     * @brief Pretty prints a Block Info object
     *
     */
    void _print_block_info ();

    /**
     * @brief Returns a copy of the readable offsets pair.
     *
     * @return pair<int, int> the cloned pair of readable offsets
     */
    pair<int, int> clone_readable_offsets ();

    /**
     * @brief Sets the max block readable index to the specified one.
     * Note: It differs from make_readable_to because it resets to the
     * specified parameter, used for truncate operations.
     *
     * @param to the max readable index
     */
    void truncate_readable_to (int to);
};

} // namespace cache::item

#endif // CACHE_ITEM_BLOCK_INFO_H