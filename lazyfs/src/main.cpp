
/**
 * @file main.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <cache/config/config.hpp>
#include <errno.h>
#include <fstream>
#include <lazyfs/lazyfs.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <unistd.h>

using namespace lazyfs;

#define MAX_READ_CHUNK 100

cache::config::Config std_config;
std::thread faults_handler_thread;
LazyFS fs;

void fht_worker (LazyFS* filesystem) {
    int fd_fifo, fd_fifo_completed;
    
    fd_fifo = open (std_config.FIFO_PATH.c_str (), O_RDWR);
    if (fd_fifo < 0) {
        spdlog::critical ("[lazyfs.fifo]: failed to open fifo '{}' (error: {})",
                          std_config.FIFO_PATH.c_str (),
                          strerror (errno));
        return;
    }

    bool write_completed_faults = (std_config.FIFO_PATH_COMPLETED != "");

    if (write_completed_faults) {
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

            if (!strcmp (buffer, "lazyfs::clear-cache")) {

                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                filesystem->command_fault_clear_cache ();
                
                if (write_completed_faults) {
                    const char* clear_cache = "clear-cache";
                    write(fd_fifo_completed, clear_cache, strlen(clear_cache));
                }

            } else if (!strcmp (buffer, "lazyfs::display-cache-usage")) {

                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                filesystem->command_display_cache_usage ();

            } else if (!strcmp (buffer, "lazyfs::cache-checkpoint")) {

                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                filesystem->command_checkpoint ();

            } else {

                spdlog::info ("[lazyfs.faults.worker]: command unknown '{}'", string (buffer));
            }

        } else
            break;
    }

    spdlog::info ("[lazyfs.faults.worker]: worker stopped");

    close (fd_fifo);   
    if (write_completed_faults) close(fd_fifo_completed);
}

int main (int argc, char* argv[]) {

    // Parse command line args

    enum { MAX_ARGS = 10 };
    int i, new_argc;
    char* new_argv[MAX_ARGS];

    string config_path;
    bool path_specified = false;

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

    std_config.load_config (config_path);

    // Setup logger

    bool only_console_sink = false;

    if (std_config.LOG_FILE != "") {

        std::ofstream outLogFile (std_config.LOG_FILE);

        if (outLogFile) {

            outLogFile.flush ();
            outLogFile.close ();

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt> ();
            console_sink->set_level (spdlog::level::info);

            auto file_sink =
                std::make_shared<spdlog::sinks::basic_file_sink_mt> (std_config.LOG_FILE, false);
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
                spdlog::info ("[lazyfs.fifo]: faults fifo exists!");
        } else
            spdlog::info ("[lazyfs.fifo]: fifo {} created", std_config.FIFO_PATH_COMPLETED.c_str ());
    }

    // Create engine and cache objects

    CustomCacheEngine* engine = new CustomCacheEngine (&std_config);
    Cache* cache              = new Cache (&std_config, engine);

    new (&fs) LazyFS (cache, &std_config, &faults_handler_thread, fht_worker);

    spdlog::info ("[lazyfs.fifo]: running LazyFS...");

    // Start LazyFS

    int status = fs.run (new_argc, new_argv);

    return status;
}