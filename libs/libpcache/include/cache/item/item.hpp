
#ifndef CACHE_ITEM_HPP
#define CACHE_ITEM_HPP

#include <cache/item/data.hpp>
#include <cache/item/metadata.hpp>
#include <iostream>
#include <utility>

using namespace std;

namespace cache::item {

class Item {
  private:
    Data* data;
    Metadata* metadata;

  public:
    Item ();
    ~Item ();
    void update_metadata (Metadata new_meta, vector<string> values_to_update);
    Metadata* get_metadata ();
    Data* get_data ();
    void print_item ();
};

} // namespace cache::item

#endif // CACHE_ITEM_HPP