
/**
 * @file lazyfs.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <dirent.h>
#include <iostream>
#include <map>
#include <regex>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <vector>
#include <filesystem>

// LazyFS specific imports
#include <cache/cache.hpp>
#include <cache/config/config.hpp>
#include <cache/constants/constants.hpp>
#include <cache/engine/backends/custom/custom_cache.hpp>
#include <lazyfs/fusepp/Fuse-impl.h>
#include <lazyfs/lazyfs.hpp>
#include <faults_handler.hpp>

using namespace std;
using namespace cache;
using namespace cache::engine::backends::custom;

std::shared_mutex cache_command_lock;

namespace lazyfs {

/* LazyFS class */

LazyFS::LazyFS () {}

LazyFS::LazyFS (Cache* cache,
                cache::config::Config* config,
                std::thread* faults_handler_thread,
                unordered_map<string, vector<faults::Fault*>>* faults,
                string mount_dir,
                string root_dir) {

    this->FSConfig              = config;
    this->FSCache               = cache;
    this->faults_handler_thread = faults_handler_thread;
    this->faults                = faults;
    this->mount_dir             = mount_dir;
    this->root_dir              = root_dir;


    this->pending_write         = NULL;
    this->kill_before.          store (false);
    this->snapshot_counter.     store(0);

    for (auto const& it : faults::Fault::allow_clear_fs_operations) {
        this->crash_faults_before_map.insert ({it, {}});
        this->crash_faults_after_map.insert ({it, {}});
    }
}

LazyFS::~LazyFS () {}

} // namespace lazyfs
