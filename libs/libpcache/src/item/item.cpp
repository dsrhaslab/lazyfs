
/**
 * @file item.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <cache/item/item.hpp>
#include <cache/item/metadata.hpp>

namespace cache::item {

Item::Item () {

    this->data        = new Data ();
    this->metadata    = new Metadata ();
    this->data_synced = true;
}

Item::~Item () {
    delete this->data;
    delete this->metadata;
}

void Item::print_item () {
    cout << "\tdata:\n";
    this->data->print_data ();
    cout << "\tmetadata:\n";
    this->metadata->print_metadata ();
}

void Item::update_metadata (Metadata new_meta, vector<string> values_to_update) {

    Metadata* oldmeta = get_metadata ();

    for (auto const& str : values_to_update) {

        if (str == "size")
            oldmeta->size = new_meta.size;
        else if (str == "atime")
            oldmeta->atim = new_meta.atim;
        else if (str == "ctime")
            oldmeta->ctim = new_meta.ctim;
        else if (str == "mtime")
            oldmeta->mtim = new_meta.mtim;
        else if (str == "nlinks")
            oldmeta->nlinks = new_meta.nlinks;
    }
}

Metadata* Item::get_metadata () { return this->metadata; }

Data* Item::get_data () { return this->data; }

void Item::set_data_sync_flag (bool flag) { this->data_synced = flag; }

bool Item::is_synced () { return this->data_synced; }

} // namespace cache::item
