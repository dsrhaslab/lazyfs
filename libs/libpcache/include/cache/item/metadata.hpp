
/**
 * @file metadata.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef CACHE_ITEM_METADATA_HPP
#define CACHE_ITEM_METADATA_HPP

#include <sys/types.h>
#include <time.h>

namespace cache::item {

/**
 * @brief Stores the Item metadata values
 *
 */

class Metadata {

  public:
    /**
     * @brief Number of hard links associated with the inode
     *
     */
    nlink_t nlinks;
    /**
     * @brief Item size in bytes
     *
     */
    off_t size;
    /**
     * @brief Item access time
     *
     */
    struct timespec atim;
    /**
     * @brief Item modify time
     *
     */
    struct timespec mtim;
    /**
     * @brief Item change time
     *
     */
    struct timespec ctim;

    /**
     * @brief Construct a new Metadata object
     *
     */
    Metadata ();

    /**
     * @brief Pretty prints a metadata object
     *
     */
    void print_metadata ();
};

} // namespace cache::item

#endif // CACHE_ITEM_METADATA_HPP