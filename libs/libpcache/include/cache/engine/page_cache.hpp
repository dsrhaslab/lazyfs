
/**
 * @file page_cache.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef CACHE_ENGINE_HPP
#define CACHE_ENGINE_HPP

#include <map>
#include <string>
#include <sys/types.h>
#include <unordered_map>

using namespace std;

namespace cache::engine {

/**
 * @brief Specifies the type of operation passed to the cache put method.
 * The type of operation is associated with the page dirty state. If
 * OP_WRITE is passed, the allocated page for a certain is marked dirty,
 * which does not happen for OP_READ or OP_PASSTHROUGH (this last two
 * enum values will behave the same for now, there is no difference in
 * program logic)
 *
 */
enum AllocateOperationType {
    OP_READ,       /**< Specifies that the put operation comes from a read operation */
    OP_WRITE,      /**< Specifies that the put operation comes from a write operation */
    OP_PASSTHROUGH /**< Specifies the otherwise case (equal to OP_READ for now) */
};

/**
 * @brief This class is an abstraction for data blocks management in the cache. It
 * represents a page cache interface.
 *
 */
class PageCacheEngine {

  public:
    /**
     * @brief Construct a new Page Cache Engine object
     *
     */
    PageCacheEngine () {};

    /**
     * @brief Destroy the Page Cache Engine object
     *
     */
    virtual ~PageCacheEngine () {};

    /**
     * @brief The allocation method that tries to allocate the specified blocks in the cache if
     * space is available.
     *
     * @param content_owner_id the content id (which is the actual page owner)
     * @param block_data_mapping block ids mapped to (last page id if exists or -1, data buffer,
     * buffer size, start offset)
     * @param operation_type the callee operation type
     * @return map<int, int> each requested block mapped to the allocated page or -1 if the
     * allocation failed
     */
    virtual map<int, int>
    allocate_blocks (string content_owner_id,
                     map<int, tuple<int, const char*, size_t, int>> block_data_mapping,
                     AllocateOperationType operation_type) = 0;

    /**
     * @brief The get method that tries to get blocks stored in a certain cache page
     *
     * @param content_owner_id the content id
     * @param block_pages each requested block mapped to (page stored, data buffer to store bytes,
     * max index within the block)
     * @return map<int, bool> each block mapped to a boolean meaning: 'block was found and data
     * buffer is filled with readable bytes'
     */
    virtual map<int, bool> get_blocks (string content_owner_id,
                                       map<int, tuple<int, char*, int>> block_pages) = 0;

    /**
     * @brief Checks whether a block is cached in the page cache
     *
     * @param content_owner_id the content id
     * @param page_id the page id where the block was stored
     * @param block_id the block id
     * @return true the block was found in that page
     * @return false the block was not found or the owner was evicted
     */
    virtual bool is_block_cached (string content_owner_id, int page_id, int block_id) = 0;

    /**
     * @brief Sets the max readable offset for a block in a page
     *
     * @param cid the content id
     * @param page_id the page id
     * @param block_id the block id
     * @param offset the max readable offset
     */
    virtual void
    make_block_readable_to_offset (string cid, int page_id, int block_id, int offset) = 0;

    /**
     * @brief Pretty prints the page cache contents
     *
     */
    virtual void print_page_cache_engine () = 0;

    /**
     * @brief Get the engine usage percentage (used pages / total pages)
     *
     * @return double the cache usage
     */
    virtual double get_engine_usage () = 0;

    /**
     * @brief Removes the cached blocks associated with an owner. Note, this
     * operation does not sync any dirty data.
     *
     * @param content_owner_id the content id
     * @return true the content was found and removed
     * @return false the content was not found
     */
    virtual bool remove_cached_blocks (string content_owner_id) = 0;

    /**
     * @brief Syncs the blocks associated with an owner with the underlying filesystem.
     * It calls pwritev for the dirty data chunks but fsync is not issued, for performance
     * reasons.
     *
     * @param owner the content id
     * @return true the pages were synced
     * @return false the content was not found
     */
    virtual bool sync_pages (string owner, off_t size, char* orig_path) = 0;

    /**
     * @brief Renames the content associated with an owner to the new owner
     *
     * @param old_owner
     * @param new_owner
     * @return true
     * @return false
     */
    virtual bool rename_owner_pages (string old_owner, string new_owner) = 0;

    /**
     * @brief Truncates cached blocks from the block id specified and the index within the block.
     * Fills the remaining data within the block with zeros.
     *
     * @param content_owner_id the content id
     * @param blocks_to_remove the block ids mapped to the last stored page id
     * @param from_block_id the block where truncation should start
     * @param index_inside_block the index withing the block
     * @return true
     * @return false
     */
    virtual bool truncate_cached_blocks (string content_owner_id,
                                         unordered_map<int, int> blocks_to_remove,
                                         int from_block_id,
                                         off_t index_inside_block) = 0;
};

} // namespace cache::engine

#endif // CACHE_ENGINE_HPP