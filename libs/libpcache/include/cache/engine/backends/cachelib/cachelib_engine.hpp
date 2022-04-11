#ifndef CACHE_ENGINE_CACHELIB_HPP
#define CACHE_ENGINE_CACHELIB_HPP

#include <cache/engine/page_cache.hpp>
#include <unordered_map>

namespace cache::engine::backends::cachelib {

// class CacheLibEngine : public PageCacheEngine {
class CacheLibEngine {
  public:
    CacheLibEngine ();
    ~CacheLibEngine ();
    unordered_map<int, int>
    allocate_blocks (string content_owner_id,
                     // block_id -> (allocated_page (or -1 if does not exist)), data, data_length)
                     unordered_map<int, tuple<int, const void*, size_t>> block_data_mapping);
};

} // namespace cache::engine::backends::cachelib

#endif // CACHE_ENGINE_CACHELIB_HPP