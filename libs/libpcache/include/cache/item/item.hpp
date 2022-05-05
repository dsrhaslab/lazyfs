
/**
 * @file item.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef CACHE_ITEM_HPP
#define CACHE_ITEM_HPP

#include <cache/item/data.hpp>
#include <cache/item/metadata.hpp>
#include <iostream>
#include <utility>

using namespace std;

namespace cache::item {

/**
 * @brief Represents an Item, e.g. a file, mapped to its data and metadata information.
 *
 */

class Item {
  private:
    /**
     * @brief Item data
     *
     */
    Data* data;

    /**
     * @brief Item Metadata
     *
     */
    Metadata* metadata;

  public:
    /**
     * @brief Construct a new Item object
     *
     */
    Item ();

    /**
     * @brief Destroy the Item object
     *
     */
    ~Item ();

    /**
     * @brief Updates the metadata values from the provided Metadata object.
     * The values to update should be passed as a list of strings.
     * Example: ["size", "mtime", "atime", "ctime"] and this method will read the values
     * from the object provided, otherwise will be ignored.
     *
     * @param new_meta the Metadata object to read from
     * @param values_to_update a list of values to update
     */
    void update_metadata (Metadata new_meta, vector<string> values_to_update);

    /**
     * @brief Get the metadata object
     *
     * @return Metadata* a reference to the metadata
     */
    Metadata* get_metadata ();

    /**
     * @brief Get the data object
     *
     * @return Data* a reference to the data
     */
    Data* get_data ();

    /**
     * @brief Pretty prints an Item object
     *
     */
    void print_item ();
};

} // namespace cache::item

#endif // CACHE_ITEM_HPP