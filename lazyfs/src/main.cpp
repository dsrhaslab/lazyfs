
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
#include <regex>
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
    std::shared_mutex fifo_lock;
    
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

            std::string command_str = string (buffer);
            if (command_str.rfind ("lazyfs::crash", 0) == 0) {

                std::regex rgx_global ("::");
                std::regex rgx_attrib ("=");
                std::sregex_token_iterator iter_glob (command_str.begin (),
                                                      command_str.end (),
                                                      rgx_global,
                                                      -1);
                std::sregex_token_iterator end;

                string crash_operation = "none";
                string crash_timing    = "none";
                string crash_from_rgx  = "none";
                string crash_to_rgx    = "none";

                // VF = Valid fault
                bool VF = true;
                vector<string> errors;

                for (; iter_glob != end; ++iter_glob) {

                    string current = string (*iter_glob);

                    if (current.rfind ("op=", 0) == 0) {

                        string tmp_op = current.erase (0, current.find ("=") + 1);

                        if (tmp_op.length () != 0 &&
                            filesystem->allow_crash_fs_operations.find (tmp_op) !=
                                filesystem->allow_crash_fs_operations.end ())
                            crash_operation = tmp_op;
                        else {
                            VF = false;
                            errors.push_back ("operation not available");
                        }

                    } else if (current.rfind ("timing=", 0) == 0) {

                        string tmp_tim = current.erase (0, current.find ("=") + 1);

                        if (tmp_tim.length () != 0 && (tmp_tim == "before" || tmp_tim == "after"))
                            crash_timing = tmp_tim;
                        else {
                            errors.push_back ("timing should be 'before' or 'after'");
                            VF = false;
                        }

                    } else if (current.rfind ("from_rgx=", 0) == 0) {

                        string tmp_from_rgx = current.erase (0, current.find ("=") + 1);

                        if (tmp_from_rgx.length () != 0)
                            crash_from_rgx = tmp_from_rgx;

                    } else if (current.rfind ("to_rgx=", 0) == 0) {

                        string tmp_to_rgx = current.erase (0, current.find ("=") + 1);

                        if (tmp_to_rgx.length () != 0)
                            crash_to_rgx = tmp_to_rgx;
                    }
                }

                bool is_from_to = false;

                if (filesystem->fs_op_multi_path.find (crash_operation) !=
                    filesystem->fs_op_multi_path.end ()) {

                    if (crash_from_rgx == "none" && crash_to_rgx == "none") {
                        errors.push_back ("should specify 'from_rgx' and/or 'to_rgx'");
                        VF = false;
                    }

                    is_from_to = true;

                } else {

                    if (crash_from_rgx == "none" || crash_to_rgx != "none") {
                        VF = false;
                        errors.push_back ("should specify 'from_rgx' regex (and not 'to_rgx')");
                    }
                }

                if (VF) {

                    spdlog::critical ("[lazyfs.faults.worker]: received: VALID crash fault:");
                    spdlog::critical ("[lazyfs.faults.worker]: => crash: timing = {}",
                                      crash_timing);
                    spdlog::critical ("[lazyfs.faults.worker]: => crash: operation = {}",
                                      crash_operation);
                    spdlog::critical ("[lazyfs.faults.worker]: => crash: from regex path = {}",
                                      crash_from_rgx);
                    if (is_from_to)
                        spdlog::critical ("[lazyfs.faults.worker]: => crash: to regex path = {}",
                                          crash_to_rgx);

                    filesystem->add_crash_fault (crash_timing,
                                                 crash_operation,
                                                 crash_from_rgx,
                                                 crash_to_rgx);

                } else {

                    spdlog::warn ("[lazyfs.faults.worker]: received: INVALID crash fault:");

                    for (auto const err : errors) {
                        spdlog::warn ("[lazyfs.faults.worker]: crash fault error: {}", err);
                    }
                }

            } else if (!strcmp (buffer, "lazyfs::clear-cache")) {

                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                filesystem->command_fault_clear_cache ();
                
                if (write_completed_faults) {
                    const char* clear_cache = "finished::clear-cache\n";
                    fifo_lock.lock();
                    if ((ret = write(fd_fifo_completed, clear_cache, strlen(clear_cache))) < 0) {
                        spdlog::warn ("[lazyfs.faults.worker]: failed to write to notifier fifo");
                    };
                    fifo_lock.unlock();
                }

            } else if (command_str.rfind ("lazyfs::torn-op", 0) == 0) {
                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                
                std::regex rgx_global ("::");
                std::regex rgx_attrib ("=");
                std::sregex_token_iterator iter_glob (command_str.begin (),
                                                      command_str.end (),
                                                      rgx_global,
                                                      -1);
                std::sregex_token_iterator end;

                string file = "none";
                string parts = "none";
                string parts_bytes = "none";
                string persist = "none";

                bool valid_fault = true;
                vector<string> errors;

                for (; iter_glob != end; ++iter_glob) {

                    string current = string (*iter_glob);

                    spdlog::info ("[lazyfs.faults.worker]: {}",current);

                    if (current.rfind ("file=", 0) == 0) {

                        string tmp_file = current.erase (0, current.find ("=") + 1);

                        if (tmp_file.length () != 0)
                            file = tmp_file;
                        else {
                            errors.push_back ("file not specified");
                            valid_fault = false;
                        }

                    } else if (current.rfind ("parts=", 0) == 0) {

                        string tmp_parts = current.erase (0, current.find ("=") + 1);
                        std::regex pattern(R"(\d+)");

                        if (!std::regex_match(tmp_parts, pattern)) {
                            errors.push_back ("parts should be a number");
                            valid_fault = false;
                        } else 
                            parts = tmp_parts;
                        
                    
                    } else if (current.rfind ("parts_bytes=", 0) == 0) {
                        
                        string tmp_parts_bytes = current.erase (0, current.find ("=") + 1);
                        std::regex pattern(R"((\d+,)*\d+)");

                        if (!std::regex_match(tmp_parts_bytes, pattern)) {
                            errors.push_back ("parts_bytes should be a list of numbers separated by commas");
                            valid_fault = false;
                        } else 
                            parts_bytes = tmp_parts_bytes;
                        

                    } else if (current.rfind ("persist=", 0) == 0) {

                        string tmp_persist = current.erase (0, current.find ("=") + 1);
                        std::regex pattern(R"((\d+,)*\d+)");

                        if (!std::regex_match(tmp_persist, pattern)) {
                            errors.push_back ("persist should be a list of numbers separated by commas");
                            valid_fault = false;
                        } else 
                            persist = tmp_persist;
                        
                    } else if (current != "lazyfs" && current != "torn-op") {
                        errors.push_back ("unknown attribute");
                        valid_fault = false;
                    }
                }

                if (parts=="none" && parts_bytes=="none") {
                    errors.push_back ("should specify 'parts' or 'parts_bytes', not both");
                    valid_fault = false;
                }
                
                vector<string> errors_add_torn_op;
                if (valid_fault) 
                    errors_add_torn_op = filesystem->add_torn_op_fault (file, parts, parts_bytes, persist);

                if (errors_add_torn_op.size() == 0) 
                        spdlog::info ("[lazyfs.faults.worker]: configured successfully '{}'", string (buffer));
                else {
                    spdlog::warn ("[lazyfs.faults.worker]: received: INVALID torn-op fault:");

                    errors.insert(errors.end(), errors_add_torn_op.begin(), errors_add_torn_op.end());

                    for (auto const err : errors) {
                        spdlog::warn ("[lazyfs.faults.worker]: {}", err);
                    }
                    
                }
                
            } else if (command_str.rfind ("lazyfs::torn-seq", 0) == 0) {
                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                
                std::regex rgx_global ("::");
                std::regex rgx_attrib ("=");
                std::sregex_token_iterator iter_glob (command_str.begin (),
                                                      command_str.end (),
                                                      rgx_global,
                                                      -1);
                std::sregex_token_iterator end;

                string file = "none";
                string op = "none";
                string persist = "none";

                bool valid_fault = true;
                vector<string> errors;

                for (; iter_glob != end; ++iter_glob) {

                    string current = string (*iter_glob);

                    if (current.rfind ("file=", 0) == 0) {

                        string tmp_file = current.erase (0, current.find ("=") + 1);

                        if (tmp_file.length () != 0)
                            file = tmp_file;
                        else {
                            errors.push_back ("bad file specification");
                            valid_fault = false;
                        }

                    } else if(current.rfind ("op=", 0) == 0) {

                        string tmp_op = current.erase (0, current.find ("=") + 1);

                        if (tmp_op.length () != 0)
                            op = tmp_op;
                        else {
                            errors.push_back ("operation not available");
                            valid_fault = false;
                        }
                    } else if (current.rfind ("persist=", 0) == 0) {

                        string tmp_per = current.erase (0, current.find ("=") + 1);
                        std::regex pattern(R"((\d+,)*\d+)");

                        if (!std::regex_match(tmp_per, pattern)) {
                            errors.push_back ("persist should be a list of numbers separated by commas");
                            valid_fault = false;
                        } else 
                            persist = tmp_per;
                        
                    } else if (current != "lazyfs" && current != "torn-seq") {
                        errors.push_back ("unknown attribute");
                        valid_fault = false;
                    }
                }

                vector<string> errors_add_torn_seq;
                if (valid_fault) 
                    errors_add_torn_seq = filesystem->add_torn_seq_fault(file, op, persist);

                if (errors_add_torn_seq.size() == 0)
                        spdlog::info ("[lazyfs.faults.worker]: configured successfully '{}'", string (buffer));
                else {  
                        errors.insert(errors.end(), errors_add_torn_seq.begin(), errors_add_torn_seq.end());

                        spdlog::warn ("[lazyfs.faults.worker]: received: INVALID torn-seq fault:");
                        
                        for (auto const err : errors) {
                            spdlog::warn ("[lazyfs.faults.worker]: {}", err);
                        }
                }      
            
            } else if (!strcmp (buffer, "lazyfs::display-cache-usage")) {

                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                filesystem->command_display_cache_usage ();

            } else if (!strcmp (buffer, "lazyfs::cache-checkpoint")) {

                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                filesystem->command_checkpoint ();

            } else if (!strcmp (buffer, "lazyfs::unsynced-data-report")) {

                spdlog::info ("[lazyfs.faults.worker]: received '{}'", string (buffer));
                vector<string> injecting_fault = filesystem->get_injecting_fault ();
                filesystem->command_unsynced_data_report (injecting_fault);
                
            } else if (!strcmp (buffer, "lazyfs::help")) {

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
                spdlog::info ("[lazyfs.fifo]: faults notifier fifo exists!");
        } else
            spdlog::info ("[lazyfs.fifo]: fifo {} created", std_config.FIFO_PATH_COMPLETED.c_str ());
    }

    // Create engine and cache objects

    CustomCacheEngine* engine = new CustomCacheEngine (&std_config);
    Cache* cache              = new Cache (&std_config, engine);

    new (&fs) LazyFS (cache, &std_config, &faults_handler_thread, fht_worker, &faults);

    spdlog::info ("[lazyfs.fifo]: running LazyFS...");

    // Start LazyFS

    int status = fs.run (new_argc, new_argv);

    return status;
}
