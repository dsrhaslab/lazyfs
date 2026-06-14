
/**
 * @file lazyfs_faults.cpp
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

extern std::shared_mutex cache_command_lock;

namespace lazyfs {

vector<string> LazyFS::get_injecting_fault () {
    lock_guard<mutex> guard (this->injecting_fault_lock);
    return injecting_fault;
}

bool LazyFS::trigger_crash_fault (string opname,
                                  string optiming,
                                  string from_op_path,
                                  string to_op_path,
                                  bool lock_needed) {

    vector<pair<std::regex, string>> opfaults;

    /*
       const int length = from_op_path.length();
       // declaring character array (+1 for null terminator)
       char* char_array = new char[length + 1];

       // copying the contents of the
       // string to char array
       strcpy(char_array, from_op_path.c_str());
       struct stat buffer;
       int status;

       status = lstat(char_array, &buffer);
   */

    if (optiming == "before") {
        opfaults = this->crash_faults_before_map.at (opname);
    } else {
        opfaults = this->crash_faults_after_map.at (opname);
    }

    if (opfaults.size () > 0) {

        bool is_multi_path = false;
        if (faults::Fault::fs_op_multi_path.find (opname) !=
            faults::Fault::fs_op_multi_path.end ()) {
            is_multi_path = true;
        }

        for (auto const& it : opfaults) {

            bool regex_match = true;

            auto const& from_rgx = it.first;

            if (is_multi_path) {

                string to = it.second;
                std::regex to_rgx (".*" + to + ".*");

                if (std::regex_match ("none", from_rgx))
                    regex_match = std::regex_match (from_op_path, from_rgx);
                if (to != "none")
                    regex_match = regex_match && std::regex_match (to_op_path, to_rgx);

            } else {

                regex_match = std::regex_match (from_op_path, from_rgx);
            }

            if (regex_match) {

                spdlog::critical ("Triggered fault condition (op={},timing={})", opname, optiming);

                this->injecting_fault_lock.lock ();
                this_ ()->command_unsynced_data_report (this->injecting_fault);
                this->injecting_fault_lock.unlock ();

                //Taking snapshots before killing LazyFS
                if (this->snapshot_counter.load () >= 0) {
                    this_() ->command_snapshot_files (regex(this->FSConfig->SNAPSHOT_FILES), this->FSConfig->SNAPSHOT_SAVE, lock_needed);
                }

                pid_t lazyfs_pid = getpid ();
                spdlog::critical ("Killing LazyFS pid {}!", lazyfs_pid);
                kill (lazyfs_pid, SIGKILL);
            }
        }
    }
    return false;
}

bool LazyFS::handle_clear_fault (faults::ClearF* clear_fault, const string& opname, const string& optiming, const string& to_path, bool lock_needed) {
    bool is_multi_path = faults::Fault::fs_op_multi_path.find (opname) != faults::Fault::fs_op_multi_path.end ();

    if (!((is_multi_path && to_path == clear_fault->to) || !is_multi_path))
        return false;

    int current_count = clear_fault->counter.load ();
    if (optiming == "before") {
        clear_fault->counter.fetch_add (1);
        current_count++;
    }

    if (clear_fault->timing != optiming || current_count != clear_fault->occurrence)
        return false;

    spdlog::critical ("Triggered fault condition (op={},timing={})", opname, optiming);

    this->injecting_fault_lock.lock ();
    this_ ()->command_unsynced_data_report (this->injecting_fault);
    this->injecting_fault_lock.unlock ();

    if (clear_fault->crash) {
        if (optiming == "after" && clear_fault->ret) {
            this_ ()->kill_before.store (true);
            spdlog::warn ("Syscall {} will return. LazyFS will crash before the next system call.", opname);
            return true;
        }
        pid_t lazyfs_pid = getpid ();
        spdlog::critical ("Killing LazyFS pid {}!", lazyfs_pid);
        kill (lazyfs_pid, SIGKILL);
    } else {
        spdlog::warn ("[lazyfs.cmds]: clearing cache...");
        this_ ()->command_fault_clear_cache (lock_needed);
    }
    return false;
}

bool LazyFS::handle_sync_pages_configured_fault (faults::SyncPagesF* page_fault, const string& opname, const string& optiming, const string& to_path, bool lock_needed) {
    bool is_multi_path = faults::Fault::fs_op_multi_path.find (opname) != faults::Fault::fs_op_multi_path.end ();

    if (!((is_multi_path && to_path == page_fault->to) || !is_multi_path))
        return false;

    int current_count = page_fault->counter.load ();
    if (optiming == "before") {
        current_count = page_fault->counter.fetch_add (1);
    }

    if (page_fault->timing != optiming || current_count != page_fault->occurrence)
        return false;

    spdlog::critical ("Triggered fault condition (op={},timing={})", opname, optiming);

    this->injecting_fault_lock.lock ();
    this_ ()->command_unsynced_data_report (this->injecting_fault);
    this->injecting_fault_lock.unlock ();

    if (optiming == "after" && page_fault->ret) {
        this_ ()->kill_before.store (true);
        spdlog::warn ("Syscall {} will return. LazyFS will crash before the next system call.", opname);
        return true;
    }

    if (this->snapshot_counter.load () >= 0) {
        this_ ()->command_snapshot_files (regex (this->FSConfig->SNAPSHOT_FILES), this->FSConfig->SNAPSHOT_SAVE, lock_needed);
    }

    pid_t lazyfs_pid = getpid ();
    spdlog::critical ("Killing LazyFS pid {}!", lazyfs_pid);
    kill (lazyfs_pid, SIGKILL);
    return false;
}

// TO-DO: Unify this with trigger_crash_fault. This function gets the faults in the LazyFS fault
// structure, and the other functions gets in the crash_faults_before_map and
// crash_faults_after_map.
bool LazyFS::trigger_configured_clear_fault (string opname,
                                             string optiming,
                                             string from_path,
                                             string to_path,
                                             bool lock_needed) {


    auto it = faults->find (from_path);

    if (it != faults->end ()) {
        auto& v_faults = it->second;

        for (auto fault : v_faults) {
            faults::ClearF* clear_fault    = dynamic_cast<faults::ClearF*> (fault);
            faults::SyncPagesF* page_fault = dynamic_cast<faults::SyncPagesF*> (fault);

            if (clear_fault && clear_fault->op == opname)
                if (handle_clear_fault (clear_fault, opname, optiming, to_path, lock_needed))
                    return true;

            if (page_fault && page_fault->op == opname)
                if (handle_sync_pages_configured_fault (page_fault, opname, optiming, to_path, lock_needed))
                    return true;
        }
    }
    return false;
}

void LazyFS::trigger_sync_pages_fault(faults::SyncPagesF &sync_pages) {

    string inode =  this->FSCache->get_original_inode (sync_pages.file);

    if (inode.empty ()) {
        spdlog::error ("[lazyfs.cmd]: sync pages fault: file {} has no inode mapping!", sync_pages.file);
        return;
    }

    bool synced = this->FSCache->partial_sync_owner (inode, sync_pages);

    if (!synced)
        spdlog::warn ("[lazyfs.cmd]: sync pages went wrong!");
    else {
        spdlog::info ("[lazyfs.cmd]: sync pages successfull!");

        if (sync_pages.sync_other_files) {
            spdlog::warn (
                "[lazyfs.{}]: sync other files is enabled, proceeding to sync other files...",
                SYNC_PAGES);

            vector<string> inodes = FSCache->unsynced_inodes ();

            for (const auto& unsynced_inode : inodes) {
                if (unsynced_inode != inode) {

                    const auto& files = FSCache->find_files_mapped_to_inode (unsynced_inode);

                    for (const auto& file : files) {

                        synced = FSCache->sync_owner (unsynced_inode, true, const_cast<char*> (file.c_str ()));

                        if (!synced) {
                            spdlog::warn ("[lazyfs.{}]: Failed to sync file: {}", SYNC_PAGES, file);
                        } else {
                            spdlog::info ("[lazyfs.{}]: Successfully synced file: {}",
                                          SYNC_PAGES,
                                          file);
                        }
                    }
                }
            }
        }

        if (sync_pages.crash) {
            pid_t lazyfs_pid = getpid ();
            spdlog::critical ("Killing LazyFS pid {} after sync-pages!", lazyfs_pid);
            kill (lazyfs_pid, SIGKILL);
        }
    }
}



void LazyFS::add_crash_fault (string crash_timing,
                              string crash_operation,
                              string crash_regex_from,
                              string crash_regex_to) {

    // TO-DO: regex_to is not actually regex
    if (crash_timing == "before") {

        auto& crash_regex_list = crash_faults_before_map.at (crash_operation);
        std::regex from_rgx (".*" + crash_regex_from + ".*");
        crash_regex_list.push_back ({from_rgx, crash_regex_to});

    } else {

        auto& crash_regex_list = crash_faults_after_map.at (crash_operation);
        std::regex from_rgx (".*" + crash_regex_from + ".*");
        crash_regex_list.push_back ({from_rgx, crash_regex_to});
    }
}

void LazyFS::add_sync_pages_fault(const FaultParamsMap& params_map) {
    try {
        faults::SyncPagesF* sync_fault = faults::SyncPagesF::tryCreate(params_map);

        if (!sync_fault) {
            spdlog::error("Error creating SyncPages fault: returned null pointer.");
            return;
        }

        auto it = this->faults->find(sync_fault->from);
        bool can_add = true;

        if (it != this->faults->end()) {
            for (auto& fptr : it->second) {
                auto* existing = dynamic_cast<faults::SyncPagesF*>(fptr);
                if (existing && existing->equal(*sync_fault)) {
                    can_add = false;
                    spdlog::error("A similar SyncPages fault already exists for file '{}'.",
                                  sync_fault->from);
                    break;
                }
            }
        }

        if (can_add) {
            (*this->faults)[sync_fault->from].push_back(sync_fault);
        } else {
            delete sync_fault;
        }

    } catch (const std::exception& e) {
        spdlog::error("Error creating SyncPages fault: {}", e.what());
        return;
    }
}

void LazyFS::print_faults () {
    for (const auto& fault_pair : *(this_ ()->faults)) {
        cout << "\n>> Faults for path: " << fault_pair.first << endl;
        for (const auto& fault : fault_pair.second) {
            fault->pretty_print ();
        }
    }
    cout << "\n>> Crash Faults Before Map:" << endl;
    for (const auto& pair : this_ ()->crash_faults_before_map) {
        cout << "Operation: " << pair.first << endl;
        for (const auto& fault : pair.second) {
            cout << "  From Regex: " << fault.first.mark_count () << ", To Regex: " << fault.second
                 << endl;
        }
    }

    cout << "\n>> Crash Faults After Map:" << endl;
    for (const auto& pair : this_ ()->crash_faults_after_map) {
        cout << "Operation: " << pair.first << endl;
        for (const auto& fault : pair.second) {
            cout << "  From Regex: " << fault.first.mark_count () << ", To Regex: " << fault.second
                 << endl;
        }
    }
}

} // namespace lazyfs
