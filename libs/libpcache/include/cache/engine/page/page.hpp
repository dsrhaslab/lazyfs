#ifndef CACHE_ENGINE_PAGE_HPP
#define CACHE_ENGINE_PAGE_HPP

#include <array>
#include <cache/config/config.hpp>
#include <cache/constants/constants.hpp>
#include <cache/engine/page/block_offsets.hpp>
#include <map>
#include <string>
#include <vector>

using namespace std;

namespace cache::engine::page {

/**
 * @brief In-Memory representation of a page in the page cache.
 *
 */
class Page {

  private:
    /**
     * @brief A flag that determines if a page has unsynced data.
     *
     */
    bool is_dirty;

    /**
     * @brief The page content owner.
     *
     */
    string page_owner_id;

    /**
     * @brief A list of free indexes within the page.
     *
     */
    vector<int> free_block_indexes;

    /**
     * @brief The configuration for LazyFS.
     *
     */
    cache::config::Config* config;

    /**
     * @brief Updates the data within the specified offsets
     *
     * @param new_data
     * @param off_min
     * @param off_max
     */
    void _rewrite_offset_data (char* new_data, int off_min, int off_max);

  public:
    /**
     * @brief Page data.
     *
     */
    char* data;

    /**
     * @brief Block allocations in page.
     *
     */
    BlockOffsets allocated_block_ids;

    /**
     * @brief Construct a new Page object
     *
     * @param config LazyFS configuration
     */
    Page (cache::config::Config* config);

    /**
     * @brief Destroy the Page object
     *
     */
    ~Page ();

    /**
     * @brief Pretty print a Page object.
     *
     */
    void _print_page ();

    /**
     * @brief Checks if a block is allocated in page.
     *
     * @param block_id the block id
     * @return true the block is allocated
     * @return false the block is not allocated
     */
    bool contains_block (int block_id);

    /**
     * @brief Updates block data in the page.
     *
     * @param block_id the block id
     * @param new_data block data
     * @param data_length rewrite lenght
     * @param off_start start offset
     * @return true block updated
     * @return false block does not exist
     */
    bool update_block_data (int block_id, char* new_data, size_t data_length, int off_start);

    /**
     * @brief Get the block offsets object
     *
     * @param block_id the block id
     * @return pair<int, int> block offsets associated with the block id or (-1,-1) if not exists
     */
    pair<int, int> get_block_offsets (int block_id);

    /**
     * @brief Gets a free offset, if any, and allocates it in page
     *
     * @param block_id the block id
     * @return pair<int, int> the allocated offset
     */
    pair<int, int> get_allocate_free_offset (int block_id);

    /**
     * @brief Get the block dataobject
     *
     * @param block_id the block id
     * @param buffer buffer to store the data
     * @param read_to_max_index max index to copy
     */
    void get_block_data (int block_id, char* buffer, int read_to_max_index);

    /**
     * @brief Resets all page data structures, marking it as clean.
     *
     */
    void reset ();

    /**
     * Checks if the arg owner is the same as the page owner.
     */
    bool is_page_owner (string query);

    /**
     * @brief Changes the page owner to the new owner.
     *
     * @param new_owner the new owner
     */
    void change_owner (string new_owner);

    /**
     * @brief Gets the page owner name
     *
     * @return string the current page owner
     */
    string get_page_owner ();

    /**
     * @brief Checks if the page has free space, i.e. free blocks.
     *
     * @return true page has free space
     * @return false no blocks available
     */
    bool has_free_space ();

    /**
     * @brief Writes page data to the underlying disk.
     *
     * @return true page was synced.
     * @return false error ocurred
     */
    bool sync_data ();

    /**
     * @brief Checks if a page has the diry flag on.
     *
     * @return true
     * @return false
     */
    bool is_page_dirty ();

    /**
     * @brief Sets the max readable offset in a block.
     *
     * @param blk_id the block id
     * @param max_offset the max readable offset
     */
    void make_block_readable_to (int blk_id, int max_offset);

    /**
     * @brief Set the page as dirty
     *
     */
    void set_page_as_dirty ();

    /**
     * @brief Writes null bytes after the block and offset specified
     *
     * @param block_id the block id
     * @param from_offset the offset within the block
     */
    void write_null_from (int block_id, int from_offset);

    /**
     * @brief Removes a block from the page.
     *
     * @param block_id the block id.
     */
    void remove_block (int block_id);
};

} // namespace cache::engine::page

#endif // CACHE_ENGINE_PAGE_HPP