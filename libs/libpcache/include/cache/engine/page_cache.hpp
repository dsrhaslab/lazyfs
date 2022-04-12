#ifndef CACHE_ENGINE_HPP
#define CACHE_ENGINE_HPP

#include <map>
#include <string>
#include <sys/types.h>
#include <unordered_map>

using namespace std;

namespace cache::engine {

enum AllocateOperationType { OP_READ, OP_WRITE, OP_PASSTHROUGH };

class PageCacheEngine {

  public:
    PageCacheEngine () {};
    virtual ~PageCacheEngine () {};

    // returns block_id -> allocated_page_id
    // param block_ids: block_id -> data_to_add
    virtual map<int, int>
    // interface method
    allocate_blocks (string content_owner_id,
                     // block_id -> (allocated_page (or -1 if does not exist)), data, data_length)
                     map<int, tuple<int, const char*, size_t, int>> block_data_mapping,
                     AllocateOperationType operation_type) = 0;
    // params: (blk_id, <page_id, data_buffer>)
    virtual map<int, bool> get_blocks (string content_owner_id,
                                       map<int, tuple<int, char*, int>> block_pages)  = 0;
    virtual bool is_block_cached (string content_owner_id, int page_id, int block_id) = 0;
    virtual void
    make_block_readable_to_offset (string cid, int page_id, int block_id, int offset) = 0;
    virtual void print_page_cache_engine ()                                           = 0;
    virtual double get_engine_usage ()                                                = 0;
    virtual bool remove_cached_blocks (string content_owner_id)                       = 0;
    virtual bool sync_pages (string owner)                                            = 0;
    virtual bool rename_owner_pages (string old_owner, string new_owner)              = 0;
    virtual bool truncate_cached_blocks (string content_owner_id,
                                         unordered_map<int, int> blocks_to_remove,
                                         int from_block_id,
                                         int index_inside_block)                      = 0;
};

} // namespace cache::engine

#endif // CACHE_ENGINE_HPP