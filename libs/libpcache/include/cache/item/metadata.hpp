#ifndef CACHE_ITEM_METADATA_HPP
#define CACHE_ITEM_METADATA_HPP

#include <sys/types.h>
#include <time.h>

namespace cache::item {

class Metadata {

  public:
    off_t size;
    struct timespec atim;
    struct timespec mtim;
    struct timespec ctim;

    Metadata ();
    void print_metadata ();
};

} // namespace cache::item

#endif // CACHE_ITEM_METADATA_HPP