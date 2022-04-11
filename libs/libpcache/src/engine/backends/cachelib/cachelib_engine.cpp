
#include <cache/engine/backends/cachelib/cachelib_engine.hpp>
#include <stdio.h>
#include <tuple>
#include <unordered_map>

using namespace std;

namespace cache::engine::backends::cachelib {

CacheLibEngine::CacheLibEngine () { printf ("cachelib engine"); }

CacheLibEngine::~CacheLibEngine () {}

unordered_map<int, int>
allocate_blocks (string content_owner_id,
                 // block_id -> (allocated_page (or -1 if does not exist)), data, data_length)
                 unordered_map<int, tuple<int, const void*, size_t>> block_data_mapping) {

    (void)content_owner_id;
    (void)block_data_mapping;

    unordered_map<int, int> map;

    return map;
}

} // namespace cache::engine::backends::cachelib