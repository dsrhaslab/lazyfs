
#include <argparse/argparse.hpp>
#include <cassert>
#include <condition_variable>
#include <fcntl.h>
#include <filesystem>
#include <shared_mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;

// ---------------------------------------

#define BURST_OPS_MAX 10000
#define MAX_THREADS 100
#define MAX_WRITE_OFF 131072

// Test specification

int g_total_runtime              = 5;  // total execution time (minimum)
int g_nr_threads                 = 2;  // number of test concurrent threads
int g_clear_cache_each_n_seconds = 1;  // clear cache every N seconds
string g_lfs_mount               = ""; // path for lazyfs mount point
string g_lfs_fifo                = ""; // path for lazyfs fifo

// Test limits

int clear_cache_wait_for = 2; // seconds to wait after clearing cache
int lower_bound_write    = 0;
int upper_bound_write    = MAX_WRITE_OFF;
int max_burst_operations = BURST_OPS_MAX;

// Global loop control variables

bool stop_workers   = false;
bool last_iteration = false;
std::shared_mutex loop_mtx;
std::condition_variable_any loop_condition;
bool can_do_work        = false;
int* thread_op_count    = new int[MAX_THREADS];
bool* thread_check_file = new bool[MAX_THREADS];

// ---------------------------------------

int get_random_number (int min, int max) { return rand () % (max - min + 1) + min; }

void do_consistency_work (int tid) {

    spdlog::info ("worker {0:d}: started...", tid);

    thread_op_count[tid]   = 0;
    thread_check_file[tid] = false;

    char* file_buffer  = (char*)malloc (upper_bound_write);
    char* read_buf     = (char*)malloc (upper_bound_write);
    char* unsynced_buf = (char*)malloc (upper_bound_write);

    memset (file_buffer, 0, upper_bound_write);
    memset (unsynced_buf, 0, upper_bound_write);

    int file_size             = 0;
    int last_off_check_bottom = -1, last_off_check_up = -1;

    string worker_path = string ("file_" + to_string (tid));

    spdlog::info ("worker {0:d}: creating file {1:s}", tid, worker_path);

    int fd = open (worker_path.c_str (), O_CREAT | O_RDWR | O_TRUNC, 0777);

    if (fd < 0)
        spdlog::critical ("worker {0:d}: cloud not open {1:s}", tid, worker_path);

    while (not stop_workers) {

        std::shared_lock<std::shared_mutex> lk (loop_mtx);

        while ((not can_do_work || thread_op_count[tid] >= max_burst_operations) &&
               (not stop_workers))
            loop_condition.wait (lk);

        thread_op_count[tid]++;

        // do work

        if (thread_check_file[tid]) {

            spdlog::info ("worker {:d}: verifying contents of file with in memory buffer...", tid);

            int pr_res = pread (fd, read_buf, upper_bound_write, 0);

            spdlog::info ("worker {:d}: pread returned {} bytes, tracked file_size is {}...",
                          tid,
                          pr_res,
                          file_size);
            assert (pr_res == file_size);
            assert (!memcmp (read_buf, file_buffer, pr_res));

            thread_check_file[tid] = false;
        }

        if (last_iteration)
            break;

        // gen min and max index to write

        int bt = get_random_number (lower_bound_write, upper_bound_write - 1);
        int up = get_random_number (bt, upper_bound_write - 1);

        do {

            bt = get_random_number (lower_bound_write, upper_bound_write - 1);
            up = get_random_number (bt, upper_bound_write - 1);

        } while (bt == up);

        char ch = get_random_number (65, 90); // ascii upper case letters
        int fs  = get_random_number (0, 1);

        int write_size = up - bt + 1;

        spdlog::info ("worker {:d}: pwrite({:d}->{:d} ({} bytes),char={},fsync={})",
                      tid,
                      bt,
                      up,
                      write_size,
                      ch,
                      fs ? "on" : "off");

        char write_buf[write_size];
        memset (write_buf, ch, write_size);
        int pw_res = pwrite (fd, write_buf, write_size, bt);

        assert (pw_res == write_size);

        // read that write and check if matches

        char read_buf[write_size];
        int pr_res = pread (fd, read_buf, write_size, bt);

        assert (pr_res == write_size);
        assert (!memcmp (read_buf, write_buf, write_size));

        memset (unsynced_buf + bt, ch, write_size);

        if (last_off_check_bottom == -1)
            last_off_check_bottom = bt;

        if (last_off_check_up == -1)
            last_off_check_up = up;

        if (last_off_check_bottom != -1 && last_off_check_up != -1) {
            last_off_check_bottom = std::min (last_off_check_bottom, bt);
            last_off_check_up     = std::max (last_off_check_up, up);
        }

        if (fs) {

            spdlog::info ("worker {:d}: fsync...", tid);

            int fr = fsync (fd);
            assert (fr >= 0);

            for (int uns = last_off_check_bottom; uns <= last_off_check_up; uns++)
                file_buffer[uns] = unsynced_buf[uns];

            file_size = std::max (file_size, last_off_check_up + 1);

            last_off_check_bottom = -1;
            last_off_check_up     = -1;
        }
    }

    close (fd);

    int res_unlink = unlink (worker_path.c_str ());

    spdlog::info ("worker {0:d}: removing file {1:s}", tid, worker_path);

    if (res_unlink < 0)
        spdlog::critical ("worker {0:d}: could not unlink {1:s}", tid, worker_path);

    free (file_buffer);
    free (unsynced_buf);
    free (read_buf);
}

void do_monitoring () {

    // register start time

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now ();

    spdlog::warn ("monitor started...");

    while (true) {

        std::unique_lock<std::shared_mutex> lk (loop_mtx);

        std::chrono::time_point<std::chrono::system_clock> timenow =
            std::chrono::system_clock::now ();

        loop_condition.wait_until (lk,
                                   timenow + std::chrono::duration<double, std::ratio<1, 1000>> (
                                                 g_clear_cache_each_n_seconds * 1000));

        can_do_work = false;

        spdlog::warn ("[LOCK] monitor has now control");

        // do cache clearing work

        spdlog::warn ("monitor: clearing cache...");

        int pipefd = open (g_lfs_fifo.c_str (), O_WRONLY);
        if (pipefd > 0) {

            int r = write (pipefd, "lazyfs::clear-cache\n", 20);
            assert (r == 20);
            close (pipefd);

            // wait clear cache to finish
            std::this_thread::sleep_for (std::chrono::seconds (clear_cache_wait_for));

        } else {

            spdlog::warn ("monitor: could not open FIFO");

            can_do_work    = true;
            stop_workers   = true;
            last_iteration = true;
            loop_condition.notify_all ();
            break;
        }

        // work finished

        spdlog::warn ("[UNLOCK] monitor released control");

        for (int tid = 0; tid < g_nr_threads; tid++) {
            thread_check_file[tid] = true;
            thread_op_count[tid]   = 0;
        }

        if (std::chrono::steady_clock::now () - start > std::chrono::seconds (g_total_runtime)) {
            can_do_work    = true;
            stop_workers   = true;
            last_iteration = true;
            loop_condition.notify_all ();
            break;
        }

        can_do_work = true;

        loop_condition.notify_all ();
    }

    stop_workers = true;

    spdlog::warn ("monitor finished!");
}

int main (int argc, char** argv) {

    spdlog::info ("parsing command line args");

    // Argparser
    argparse::ArgumentParser lfscheck ("lfscheck");

    lfscheck.add_argument ("-n", "--threads")
        .help ("specifies the number of threads")
        .default_value (1)
        .scan<'i', int> ();
    lfscheck.add_argument ("-b", "--burst")
        .help ("specifies the number of writes between cache clearing")
        .default_value (100)
        .scan<'i', int> ();
    lfscheck.add_argument ("-max", "--max.off")
        .help ("specifies the max write offset")
        .default_value (4096)
        .scan<'i', int> ();

    lfscheck.add_argument ("-f", "--lfs.fifo").help ("lazyfs faults fifo path").required ();
    lfscheck.add_argument ("-m", "--lfs.mount").help ("lazyfs mount point").required ();
    lfscheck.add_argument ("-t", "--test.time")
        .help ("total minimum test time (in seconds)")
        .scan<'i', int> ()
        .required ();

    try {

        lfscheck.parse_args (argc, argv);

        if (lfscheck.is_used ("-n")) {
            g_nr_threads = lfscheck.get<int> ("-n");

            if (g_nr_threads > MAX_THREADS) {
                spdlog::warn ("threads: maximum is {0:d}", MAX_THREADS);
                std::exit (1);
            }
        }

        if (lfscheck.is_used ("-b")) {
            max_burst_operations = lfscheck.get<int> ("-b");

            if (max_burst_operations > BURST_OPS_MAX) {
                spdlog::warn ("operations burst: maximum is {0:d}", BURST_OPS_MAX);
                std::exit (1);
            }
        }

        if (lfscheck.is_used ("-max")) {
            upper_bound_write = lfscheck.get<int> ("-max");

            if (upper_bound_write > MAX_WRITE_OFF) {
                spdlog::warn ("offsets: maximum is {0:d}", MAX_WRITE_OFF);
                std::exit (1);
            }
        }

        g_lfs_fifo      = lfscheck.get<std::string> ("-f");
        g_lfs_mount     = lfscheck.get<std::string> ("-m");
        g_total_runtime = lfscheck.get<int> ("-t");

        bool path_mount_exists = std::filesystem::exists (g_lfs_mount);

        if (not path_mount_exists)
            std::cerr << "mount: path '" << g_lfs_mount << "' does not exist." << endl;

        bool path_fifo_exists = std::filesystem::exists (g_lfs_fifo);

        if (not path_fifo_exists)
            std::cerr << "fifo: path '" << g_lfs_fifo << "' does not exist." << endl;

        if (not path_fifo_exists || not path_mount_exists)
            std::exit (1);

        if (chdir (g_lfs_mount.c_str ()) < 0) {

            std::cerr << "could not change process dir to mount point" << endl;
            std::exit (1);
        }

        spdlog::info ("threads ({}), burst ({} write ops), max off ({}), total minimum time ({})",
                      g_nr_threads,
                      max_burst_operations,
                      upper_bound_write,
                      g_total_runtime);

    } catch (const std::runtime_error& err) {

        spdlog::error ("error parsing command line args");

        std::cerr << err.what () << std::endl;
        std::cerr << lfscheck;
        std::exit (1);
    }

    spdlog::info ("starting monitor thread...");

    std::thread monitor_thr (do_monitoring);

    std::vector<pair<std::thread, int>> worker_thread_pool;

    spdlog::info ("starting worker threads...");

    std::chrono::steady_clock::time_point begin_test = std::chrono::steady_clock::now ();

    for (int tid = 0; tid < g_nr_threads; tid++) {

        std::thread worker_thr (do_consistency_work, tid);
        worker_thread_pool.push_back (std::make_pair (std::move (worker_thr), tid));
    }

    for (auto& w_t : worker_thread_pool) {
        w_t.first.join ();
        spdlog::info ("worker {0:d} joined", w_t.second);
    }

    monitor_thr.join ();
    spdlog::info ("monitor joined");

    std::chrono::steady_clock::time_point end_test = std::chrono::steady_clock::now ();

    spdlog::info (
        "test took {0:d} seconds",
        std::chrono::duration_cast<std::chrono::seconds> (end_test - begin_test).count ());

    return 0;
}