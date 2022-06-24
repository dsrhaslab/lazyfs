
/**
 * @file main.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <cache/config/config.hpp>
#include <cache/util/utilities.hpp>
#include <errno.h>
#include <lazyfs/lazyfs.hpp>
#include <string>
#include <thread>
#include <unistd.h>

using namespace lazyfs;
using namespace cache::util;

#define MAX_READ_CHUNK 100

cache::config::Config std_config;
std::thread faults_handler_thread;
LazyFS fs;

void fht_worker (LazyFS* filesystem) {

    int fd;
    if ((fd = open (std_config.FIFO_PATH.c_str (), O_RDWR)) < 0) {
        _print_with_time ("[faults] fifo opening failed: (error) " + string (strerror (errno)));
        return;
    }

    _print_with_time ("[faults] worker is waiting for fault commands...");

    char buffer[MAX_READ_CHUNK];

    int ret;

    while (true) {
        if ((ret = read (fd, &buffer, MAX_READ_CHUNK)) > 0) {

            buffer[ret - 1] = '\0';

            if (!strcmp (buffer, "lazyfs::clear-cache")) {

                _print_with_time ("[command]: <" + string (buffer) + ">");
                filesystem->command_fault_clear_cache ();

            } else if (!strcmp (buffer, "lazyfs::display-cache-usage")) {

                _print_with_time ("[command]: <" + string (buffer) + ">");
                filesystem->command_display_cache_usage ();

            } else if (!strcmp (buffer, "lazyfs::cache-checkpoint")) {

                _print_with_time ("[command]: <" + string (buffer) + ">");
                filesystem->command_checkpoint ();
            } else {
                _print_with_time ("[command]: unknown <" + string (buffer) + ">");
            }

        } else
            break;
    }

    close (fd);
}

int main (int argc, char* argv[]) {

    enum { MAX_ARGS = 10 };
    int i, new_argc;
    char* new_argv[MAX_ARGS];

    string config_path;
    bool path_specified = false;
    for (i = 0, new_argc = 0; (i < argc) && (new_argc < MAX_ARGS); i++)
        if (!strcmp (argv[i], "--config-path")) {
            if (strcasestr (argv[i + 1], "-o")) {
                _print_with_time (
                    "[lazyfs] path not specified, using path 'config/default.toml'...");

            } else {
                config_path = argv[i + 1];
                _print_with_time ("[lazyfs] config path " + string (argv[i++ + 1]));
                path_specified = true;
            }
        } else
            new_argv[new_argc++] = argv[i];

    if (!path_specified)
        config_path = "config/default.toml";

    _print_with_time ("[lazyfs] starting LazyFS...");

    std_config.load_config (config_path);

    _print_with_time ("[faults] trying to create fifo " + std_config.FIFO_PATH);

    if (mkfifo (std_config.FIFO_PATH.c_str (), 0777) < 0) {
        if (errno != EEXIST) {
            _print_with_time ("[faults] could not create faults fifo: (error) " +
                              string (strerror (errno)));
            _print_with_time ("[lazyfs] stopping LazyFS...");
            return -1;
        } else
            _print_with_time ("[faults] faults fifo exists, setting up worker...");
    } else
        _print_with_time ("[faults] fifo created.");

    _print_with_time ("[lazyfs] reading configuration...");

    CustomCacheEngine* engine = new CustomCacheEngine (&std_config);
    Cache* cache              = new Cache (&std_config, engine);

    new (&fs) LazyFS (cache, &std_config, &faults_handler_thread, fht_worker);

    int status = fs.run (new_argc, new_argv);

    return status;
}