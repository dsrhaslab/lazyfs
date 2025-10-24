/**
 * @file main.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <errno.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <unistd.h>

//LazyFS specific imports
#include <cache/config/config.hpp>
#include <lazyfs/lazyfs.hpp>
#include <faults_handler.hpp>
#include <faults/faults.hpp>

using namespace lazyfs;
using namespace lazyfs_api;

#define MAX_READ_CHUNK 255

cache::config::Config std_config;
std::thread faults_handler_thread;
LazyFS fs;

void fht_worker (LazyFS* filesystem) {
    int fd_fifo, fd_fifo_completed;
    std::shared_mutex fifo_lock;

    fd_fifo = open (std_config.FIFO_PATH.c_str (), O_RDWR);
    if (fd_fifo < 0) {
        spdlog::critical ("[lazyfs.fifo]: failed to open fifo '{}' (error: {})",
                          std_config.FIFO_PATH.c_str (),
                          strerror (errno));
        return;
    }

    bool completed_fault_fifo = (std_config.FIFO_PATH_COMPLETED != "");

    if (completed_fault_fifo) {
        fd_fifo_completed = open (std_config.FIFO_PATH_COMPLETED.c_str (), O_WRONLY);
        if (fd_fifo_completed < 0) {
            spdlog::critical ("[lazyfs.fifo]: failed to open fifo '{}' (error: {})",
                            std_config.FIFO_PATH_COMPLETED.c_str (),
                            strerror (errno));
            return;
        }
    }

    spdlog::info ("[lazyfs.faults.worker]: waiting for fault commands...");

    char buffer[MAX_READ_CHUNK];
    int ret;
    while (true) {
        if ((ret = read (fd_fifo, &buffer, MAX_READ_CHUNK)) > 0) {

            buffer[ret - 1] = '\0';

            std::string command_str = string (buffer);
            spdlog::info ("[lazyfs.faults.worker]: received '{}'", command_str);

            if (command_str.rfind ("lazyfs::crash", 0) == 0) {

                string crash_operation = "none";
                string crash_timing    = "none";
                string crash_from_rgx  = "none";
                string crash_to_rgx    = "none";

                if (parse_crash(command_str, crash_timing, crash_operation, crash_from_rgx, crash_to_rgx)) {

                    filesystem->add_crash_fault (crash_timing, crash_operation, crash_from_rgx, crash_to_rgx);

                }

            } else if (command_str.rfind ("lazyfs::clear-cache", 0) == 0) {

                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                filesystem->command_fault_clear_cache ();

                if (completed_fault_fifo) {
                    const char* clear_cache = "finished::clear-cache\n";
                    fifo_lock.lock();
                    if ((ret = write(fd_fifo_completed, clear_cache, strlen(clear_cache))) < 0) {
                        spdlog::warn ("[lazyfs.faults.worker]: failed to write to notifier fifo");
                    };
                    fifo_lock.unlock();
                }

            } else if (command_str.rfind ("lazyfs::torn-op", 0) == 0) {

                string file        = "none";
                string parts       = "none";
                string parts_bytes = "none";
                string persist     = "none";
                string ret         = "none";

                if (parse_torn_op(command_str, file, parts, parts_bytes, persist, ret)) {

                    vector<string> errors_add_torn_op;
                    errors_add_torn_op = filesystem->add_torn_op_fault (file, parts, parts_bytes, persist, ret);

                    if (errors_add_torn_op.size () == 0)
                        spdlog::critical ("[lazyfs.faults.worker]: received VALID torn-op fault.");

                    else {
                        spdlog::error ("[lazyfs.faults.worker]: received: INVALID torn-op fault:");

                        for (auto const err : errors_add_torn_op) {
                            spdlog::error ("[lazyfs.faults.worker]: torn-op fault error: {}", err);
                        }
                    }
                } // else, errors already printed by parse_torn_op

            } else if (command_str.rfind ("lazyfs::torn-seq", 0) == 0) {

                string file    = "none";
                string op      = "none";
                string persist = "none";
                string ret     = "none";

                if (parse_torn_seq(command_str, file, op, persist, ret)) {

                    vector<string> errors_add_torn_seq;
                    errors_add_torn_seq = filesystem->add_torn_seq_fault(file, op, persist, ret);

                    if (errors_add_torn_seq.size() == 0)
                            spdlog::critical ("[lazyfs.faults.worker]: received VALID torn-seq fault.");

                    else {
                            spdlog::error ("[lazyfs.faults.worker]: received: INVALID torn-seq fault:");

                            for (auto const err : errors_add_torn_seq) {
                                spdlog::error ("[lazyfs.faults.worker]: torn-seq fault error: {}", err);
                            }
                    }
                } // else, errors already printed by parse_torn_seq

            } else if (command_str.rfind ("lazyfs::snapshot", 0) == 0) {

                string files = "none";
                regex files_rgx;
                string save  = "none";

                if (parse_snapshot(command_str, files, files_rgx, save))
                    filesystem->command_snapshot_files(files_rgx, save);


            } else if (!strcmp (buffer, "lazyfs::display-cache-usage")) {

                filesystem->command_display_cache_usage ();

            } else if (!strcmp (buffer, "lazyfs::cache-checkpoint")) {

                filesystem->command_checkpoint ();

            } else if (!strcmp (buffer, "lazyfs::unsynced-data-report")) {

                vector<string> injecting_fault = filesystem->get_injecting_fault ();
                filesystem->command_unsynced_data_report (injecting_fault);

            } else if (!strcmp (buffer, "lazyfs::help")) { //UPDATE

                spdlog::info ("[lazyfs.faults.worker]: <" + string (buffer) + ">");

                spdlog::info (
                    "[lazyfs.faults.worker] help: 'lazyfs::clear-cache' => clears un-fsynced data");
                spdlog::info (
                    "[lazyfs.faults.worker] help: 'lazyfs::display-cache-usage' => shows the "
                    "cache usage (#pages)");
                spdlog::info ("[lazyfs.faults.worker] help: 'lazyfs::cache-checkpoint' => writes "
                              "all cached data");
                spdlog::info (
                    "[lazyfs.faults.worker] help: 'lazyfs::unsynced-data-report' => reports which "
                    "files have un-fsynced data");
                spdlog::info (
                    "[lazyfs.faults.worker] help: 'lazyfs::help' => displays this message");

            } else {

                spdlog::info ("[lazyfs.faults.worker]: command unknown '{}'", string (buffer));
            }

        } else
            break;
    }

    spdlog::info ("[lazyfs.faults.worker]: worker stopped");

    close (fd_fifo);
    if (completed_fault_fifo) close(fd_fifo_completed);
}

int main (int argc, char* argv[]) {

    // Parse command line args

    enum { MAX_ARGS = 10 };
    int i, new_argc;
    char* new_argv[MAX_ARGS];

    string config_path;
    bool path_specified = false;

    string mount_dir;
    if (argc > 1) {
        mount_dir = argv[1];
    }

    string root_dir;
    if (argc > 9) {
        root_dir = argv[9];
        const std::string key = "subdir=";
        auto pos = root_dir.find(key);
        root_dir = root_dir.substr(pos + key.size());
    }

    for (i = 0, new_argc = 0; (i < argc) && (new_argc < MAX_ARGS); i++)

        if (!strcmp (argv[i], "--config-path")) {

            if (strcasestr (argv[i + 1], "-o"))

                path_specified = false;

            else {

                config_path    = argv[i + 1];
                path_specified = true;
                i++;
            }

        } else
            new_argv[new_argc++] = argv[i];

    // Use default config path

    if (!path_specified)
        config_path = "config/default.toml";

    // Load LazyFS's config

    unordered_map<string,vector<faults::Fault*>> faults = std_config.load_config (config_path);

    // Setup logger
    bool only_console_sink = false;

    if (std_config.LOG_FILE != "") {

        std::ofstream outLogFile (std_config.LOG_FILE);

        if (outLogFile) {

            outLogFile.flush ();
            outLogFile.close ();

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt> ();
            console_sink->set_level (spdlog::level::info);

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt> (std_config.LOG_FILE, false);
            file_sink->set_level (spdlog::level::info);

            // Sinks to console and file
            spdlog::logger logger ("global", {console_sink, file_sink});

            logger.set_level (spdlog::level::info);
            logger.flush_on (spdlog::level::info);

            // Register logger globally

            spdlog::set_default_logger (std::make_shared<spdlog::logger> (logger));

        } else {
            only_console_sink = true;
            spdlog::warn ("[lazyfs.log] could not create logfile, using stdout only");
        }

    } else
        only_console_sink = true;

    if (only_console_sink) {

        // Sinks to console

        auto console = spdlog::stdout_color_mt ("console");

        console->set_level (spdlog::level::info);
        console->flush_on (spdlog::level::info);

        spdlog::set_default_logger (console);
    }

    spdlog::info ("[lazyfs.config]: loading config...");

    if (path_specified)
        spdlog::info ("[lazyfs.args]: config path is '{}'", config_path);
    else
        spdlog::warn ("[lazyfs.args]: path not specified, using path 'config/default.toml'");


    //Fifos

    spdlog::info ("[lazyfs]: trying to create fifo '{}'", std_config.FIFO_PATH);

    // Create fifo, if not exists already
    if (mkfifo (std_config.FIFO_PATH.c_str (), 0777) < 0) {
         if (errno != EEXIST) {
             spdlog::critical ("[lazyfs.fifo]: failed to create fifo '{}' (error: {})",
                             std_config.FIFO_PATH.c_str (),
                             strerror (errno));
            spdlog::critical ("[lazyfs] exiting...");
            return -1;
        } else
             spdlog::info ("[lazyfs.fifo]: faults fifo exists!");
    } else
         spdlog::info ("[lazyfs.fifo]: fifo {} created", std_config.FIFO_PATH.c_str ());


    if (std_config.FIFO_PATH_COMPLETED != "") {
        spdlog::info ("[lazyfs]: trying to create fifo '{}'", std_config.FIFO_PATH_COMPLETED);

        // Create fifo, if not exists already
        if (mkfifo (std_config.FIFO_PATH_COMPLETED.c_str (), 0777) < 0) {
            if (errno != EEXIST) {
                spdlog::critical ("[lazyfs.fifo]: failed to create fifo '{}' (error: {})",
                                std_config.FIFO_PATH_COMPLETED.c_str (),
                                strerror (errno));
                spdlog::critical ("[lazyfs] exiting...");
                return -1;
            } else
                spdlog::info ("[lazyfs.fifo]: faults notifier fifo exists!");
        } else
            spdlog::info ("[lazyfs.fifo]: fifo {} created", std_config.FIFO_PATH_COMPLETED.c_str ());
    }

    // Create engine and cache objects

    CustomCacheEngine* engine = new CustomCacheEngine (&std_config);
    Cache* cache              = new Cache (&std_config, engine);

    new (&fs) LazyFS (cache, &std_config, &faults_handler_thread, fht_worker, &faults, mount_dir, root_dir);

    spdlog::info ("[lazyfs.fifo]: running LazyFS...");

    if (THREAD_ID) spdlog::set_pattern("[thread: %t] %+");

    // Start LazyFS

    int status = fs.run (new_argc, new_argv);

    return status;
}
