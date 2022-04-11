
#include <cache/config/config.hpp>
#include <errno.h>
#include <lazyfs/lazyfs.hpp>
#include <string>
#include <thread>
#include <unistd.h>

using namespace lazyfs;

#define MAX_READ_CHUNK 100

cache::config::Config std_config;
std::thread faults_handler_thread;
LazyFS fs;

void fht_worker (LazyFS* filesystem) {

    int fd;
    if ((fd = open (std_config.FIFO_PATH.c_str (), O_RDWR)) < 0) {
        std::printf ("\t[faults] fifo opening failed: (error) %s\n", strerror (errno));
        return;
    }

    std::printf ("\t[faults] worker is waiting for fault messages...\n");

    char buffer[MAX_READ_CHUNK];

    int ret;

    while (true) {
        if ((ret = read (fd, &buffer, MAX_READ_CHUNK)) > 0) {

            buffer[ret - 1] = '\0';

            if (!strcmp (buffer, "lazyfs::clear-cache")) {

                std::printf ("\t\t[command]: <%s>\n", buffer);
                filesystem->fault_clear_cache ();

            } else if (!strcmp (buffer, "lazyfs::display-cache-usage")) {

                std::printf ("\t\t[command]: <%s>\n", buffer);
                filesystem->display_cache_usage ();
            }

        } else
            break;
    }

    std::printf ("\t[faults] worker exited (read returned %d)...\n", ret);

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
                std::printf ("[lazyfs] path not specified, using path 'config/default.toml'...\n");

            } else {
                config_path = argv[i + 1];
                std::printf ("[lazyfs] config path %s...\n", argv[i++ + 1]);
                path_specified = true;
            }
        } else
            new_argv[new_argc++] = argv[i];

    if (!path_specified)
        config_path = "config/default.toml";

    std::printf ("[lazyfs] starting the lazy filesystem...\n");

    std_config.load_config (config_path);

    std::printf ("\t[faults] trying to create fifo %s...\n", std_config.FIFO_PATH.c_str ());

    if (mkfifo (std_config.FIFO_PATH.c_str (), 0777) < 0) {
        if (errno != EEXIST) {
            std::printf ("\t[faults] could not create faults fifo: (error) %s\n", strerror (errno));
            return -1;
        } else
            std::printf ("\t[faults] faults fifo exists, setting up worker...\n");
    } else
        std::printf ("\t[faults] fifo created.\n");

    std::printf ("[lazyfs] reading configuration...\n");

    CustomCacheEngine* engine = new CustomCacheEngine (&std_config);
    Cache* cache              = new Cache (&std_config, engine);
    new (&fs) LazyFS (cache, &std_config);

    new (&faults_handler_thread) std::thread (fht_worker, &fs);

    int status = fs.run (new_argc, new_argv);

    return status;
}