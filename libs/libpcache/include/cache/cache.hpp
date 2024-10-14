
/**
 * @file cache.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef CACHE_HPP
#define CACHE_HPP

#include <cache/config/config.hpp>
#include <cache/engine/page_cache.hpp>
#include <cache/item/item.hpp>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

using namespace std;

using namespace cache::item;
using namespace cache::engine;

namespace cache {

/**
 * @brief Cache upper layer interface, maps all contents to its data and
 * metadata values. Uses the underlying cache engine abstraction to manipulate
 * the contents data.
 *
 */

class Cache {

    /**
     * @brief Global lock for the cache.
     *        Lock order is similar to two-phase locking.
     *
     */
    std::shared_mutex lock_cache_mtx;

  private:
    /**
     * @brief Maps filenames to the corresponding inodes.
     *        If a hard link is created for a file, a new entry
     *        on this map is also created, for the same inode.
     *
     */
    unordered_map<string, string> file_inode_mapping;
    /**
     * @brief A cache configuration object
     *
     */
    cache::config::Config* cache_config;
    /**
     * @brief Maps content ids (e.g. inodes) to the contents
     *
     */
    unordered_map<string, Item*> contents;
    /**
     * @brief Maps each content to a lock mutex
     *
     */
    unordered_map<string, std::shared_mutex*> item_locks;
    /**
     * @brief Cache engine abstraction object
     *
     */
    PageCacheEngine* engine;

    /**
     * @brief Gets a content reference
     *
     * @param cid the content id
     * @return Item* the content reference object
     */
    Item* _get_content_ptr (string cid);

    /**
     * @brief Gets the readable offsets associated with an Item block
     *
     * @param cid the content id
     * @param item the Item reference
     * @param blk the block id
     * @return pair<int, int> the pair of block readable offsets
     */
    pair<int, int> _get_readable_offsets (string cid, Item* item, int blk);

  public:
    /**
     * @brief Construct a new Cache object
     *
     * @param cache_config a Config object
     * @param engine the page cache abstraction layer object
     */
    Cache (cache::config::Config* cache_config, PageCacheEngine* engine);

    /**
     * @brief Destroy the Cache object
     *
     */
    ~Cache ();

    /**
     * @brief Creates an empty item with the specified id.
     *
     * @param cid the content id
     * @return Item* the reference to the created item
     */
    Item* _create_item (string cid);

    /**
     * @brief Deletes an existing content
     *
     * @param cid the content id
     * @return true if the item was deleted
     * @return false if the item does not exist
     */
    bool _delete_item (string cid);

    /**
     * @brief Checks if a content is cached
     *
     * @param cid the content id
     * @return true the content exists
     * @return false the content does not exist
     */
    bool has_content_cached (string cid);

    /**
     * @brief Updates the content metadata
     *
     * @param cid the content id
     * @param new_meta the metadata values
     * @param values_to_update a list of value names to update
     * @return true
     * @return false
     */
    bool update_content_metadata (string cid, Metadata new_meta, vector<string> values_to_update);

    /**
     * @brief Get the content metadata object
     *
     * @param cid the content id
     * @return Metadata* a reference to the content metadata
     */
    Metadata* get_content_metadata (string cid);

    /**
     * @brief Tries to allocate the specified blocks in the cache for that content
     *
     * @param cid the content id
     * @param blocks block id mapped to the (block data, data size, start offset)
     * @param operation_type specifies if it is a write/read operation
     * @return map<int, bool> the block id mapped to a boolean specifying if the block was cached
     */
    map<int, bool> put_data_blocks (string cid,
                                    map<int, tuple<const char*, size_t, int, int>> blocks,
                                    AllocateOperationType operation_type);

    /**
     * @brief Tries to get the specified blocks from a content
     *
     * @param cid the content id
     * @param blocks a block id mapped to the pointer to store data
     * @return map<int, pair<bool, pair<int, int>>> each block id mapped to (was found?, (readable
     * from index, readable to index))
     */
    map<int, pair<bool, pair<int, int>>> get_data_blocks (string cid, map<int, char*> blocks);

    /**
     * @brief Checks if a block is cached in the engine
     *
     * @param cid the content id
     * @param blk_id the block id
     * @return true the block exists
     * @return false the block does not exist
     */
    bool is_block_cached (string cid, int blk_id);

    /**
     * @brief Pretty prints a cache object
     *
     */
    void print_cache ();

    /**
     * @brief Calls Engine pretty print method
     *
     */
    void print_engine ();

    /**
     * @brief Locks an Item (assuming it exists)
     *
     * @param cid the content id
     */
    void lockItem (string cid);

    /**
     * @brief Unlocks an Item (assuming it exists)
     *
     * @param cid the content id
     */
    void unlockItem (string cid);

    /**
     * @brief Locks the Item if it exists
     *
     * @param cid the content id
     * @return true the item was locked
     * @return false the item was not locked
     */
    bool lockItemCheckExists (string cid);

    /**
     * @brief Get the cache usage in pages
     *
     * @return double the % of pages used
     */
    double get_cache_usage ();

    /**
     * @brief Removes a cached item from cache and the data from the engine
     *
     * @param owner the content id
     * @param path original path name to remove
     * @param is_from_cache remove comes from a clear cache command
     * @return true item was removed
     * @return false item does not exist
     */
    bool remove_cached_item (string owner, const char* path, bool is_from_cache);

    /**
     * @brief Syncs a contents data and/or metadata.
     *
     * @param owner the content id
     * @param only_sync_data only sync data from content
     * @param orig_path original path to write data
     * @return bool true if data was synced
     */
    bool sync_owner (string owner, bool only_sync_data, char* orig_path);

    /**
     * @brief Renames an item id
     *
     * @param old_cid the old content id
     * @param new_cid the new content id
     * @return true if item exists
     * @return false item does not exist
     */
    bool rename_item (string old_cid, string new_cid);

    /**
     * @brief Removes all cache contents
     *
     */
    void clear_all_cache ();

    /**
     * @brief Truncate a cached item to the size specified.
     *
     * @param owner the content id
     * @param new_size the cache size
     * @return true item exists
     * @return false item does not exist
     */
    bool truncate_item (string owner, off_t new_size);

    /**
     * @brief Performs a full checkpoint for uncached data
     *
     */
    void full_checkpoint ();

    /**
     * @brief Performs a partial checkpoint for uncached data based on input from user
     *
     * @param owner the content id
     * @param path original path name
     * @param parts parts from file that will be removed
     * @return bool true if parts were synced
     * 
     */
    bool partial_file_sync (string owner, char* path, string parts);

    /**
     * @brief Gets the list of files that have unsynced data, mapped to
     *        the size cached (number of bytes).
     *
     * @return std::vector<tuple<string, size_t, vector<pair<int, pair<int, int>>>>> unsynced items
     * and their cache size
     */
    vector<tuple<string, size_t, vector<tuple<int, pair<int, int>, int>>>> report_unsynced_data ();

    /**
     * @brief Get the original inode for a file name
     *
     * @param path file name
     * @return string inode
     */
    string get_original_inode (string path);

    /**
     * @brief Inserts a new file name entry mapped to an inode.
     *
     * @param path file name
     * @param inode corresponding inode
     * @param increase to increase link counter
     */
    void insert_inode_mapping (string path, string inode, bool increase);

    /**
     * @brief Gets the list of filenames mapped to an inode
     *
     * @param inode the inode
     * @return vector<string> files mapped to that inode
     */
    vector<string> find_files_mapped_to_inode (string inode);

    /**
     * @brief Retrieves a list of unsynced inodes.
     *      
     * @return std::vector<std::string> A vector of unsynced inodes.
     */
    vector<string> unsynced_inodes();

};

} // namespace cache

#endif // CACHE_HPP