/*
 * Simple single-threaded test to exercise lazyfs "sync-pages" faults.
 * Usage: build and run from the project (or compile this file) and pass
 * -f <fifo-path> -m <mount-path> -p <relative-file-path>
 *
 * The test will create the file under the mount point, write multiple pages
 * of data without fsync, then send a sync-pages command through the FIFO
 * to instruct LazyFS to flush pages for the file.
 */

#include <argparse/argparse.hpp>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

using namespace std;


// Test specification

const size_t PAGE_SIZE = 4096;
int total_size         = PAGE_SIZE * 5; // default total size

string mount           = "";
string fifo            = "";
string rel_path        = "";      

// ---------------------------------------


int write_char(int fd, char c, size_t count) {
    char buffer[count];
    memset(buffer, c, count);

    ssize_t ret = write(fd, buffer, count);

    if (ret < 0) {
        perror(("write char '" + std::to_string(c) + "'").c_str());
        return -1;
    }

    return (int)ret;
}

int write_chars(int fd, char* buffer, size_t total_size) {
    size_t written = 0;
    char current_char = 'A';

    while (written < total_size) {
        size_t to_write = std::min(PAGE_SIZE, total_size - written);
        ssize_t w = write_char(fd, current_char, to_write);
        if (w < 0) {
            perror("write");
            return -1;
        }

        memset(buffer + written, current_char, to_write);

        current_char++;
        if (current_char > 'Z') {
            current_char = 'A';
        }
        written += (size_t)w;
    }
    return (int)written;
}

bool check_file_contents(const string& path, char* expected_buffer, size_t total_size) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open for read");
        return false;
    }

    char read_buffer[total_size];
    ssize_t r = read(fd, read_buffer, total_size);
    if (r < 0) {
        perror("read");
        close(fd);
        return false;
    }

    close(fd);

    if ((size_t)r != total_size) {
        spdlog::error("read size mismatch: expected {}, got {}", total_size, r);
        return false;
    }

    if (memcmp(read_buffer, expected_buffer, total_size) != 0) {
        spdlog::error("file contents do not match expected data");
        return false;
    }

    return true;
}

int main(int argc, char** argv) {
    argparse::ArgumentParser ap("test_sync_pages");

    ap.add_argument("-f", "--fifo").required().help("lazyfs FIFO path");
    ap.add_argument("-m", "--mount").required().help("lazyfs mount point");
    ap.add_argument("-s", "--single-threaded").default_value("true").help("run in single-threaded mode");
    ap.add_argument("-p", "--path").required().help("relative path of test file to create");
    ap.add_argument("-z", "--size").required().help("total size of test file to create (in bytes)").scan<'i', int>();

    try {
        ap.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        cerr << err.what() << "\n" << ap << endl;
        return 1;
    }

    fifo = ap.get<string>("--fifo");
    mount = ap.get<string>("--mount");
    relpath = ap.get<string>("--path");
    total_size = ap.get<int>("--size");


    if (total_size <= 0) {
        cerr << "invalid size: " << size << endl;
        return 1;
    }
    
    char buffer[total_size];

    if (!filesystem::exists(mount)) {
        cerr << "mount path does not exist: " << mount << endl;
        return 1;
    }

    if (!filesystem::exists(fifo)) {
        cerr << "fifo path does not exist: " << fifo << endl;
        return 1;
    }

    if (chdir(mount.c_str()) < 0) {
        perror("chdir: could not change process dir to mount point");
        return 1;
    }
    
    string file_path = filesystem::absolute(mount + "/" + relpath).string();

    spdlog::info("creating test file: {} ({} bytes)", file_path, total_size);

    int fd = open(file_path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open create: could not create test file");
        return 1;
    }

    int written = write_chars(fd, buffer, total_size);
    if (written < 0) {
        spdlog::error("error writing test file {}", file_path);
        return 1;
    }

    close(fd);


    

    // Construct sync-pages command: 
    // timing=now, pages=first-and-last, sync-other-files=true
    string command = "lazyfs::sync-pages::timing=now::pages=first-and-last::file=" + abs_path + "::sync-other-files=true\n";

    spdlog::info("sending command to FIFO {}: {}", fifo, command);

    int fd_fifo = open(fifo.c_str(), O_WRONLY);
    if (fd_fifo < 0) {
        perror("open fifo");
        return 1;
    }

    ssize_t rc = write(fd_fifo, command.c_str(), command.size());
    if (rc < 0) perror("write fifo");
    close(fd_fifo);

    spdlog::info("command sent, waiting {}s for LazyFS to process...", 3);
    std::this_thread::sleep_for(std::chrono::seconds(3));

    spdlog::info("test finished: file={} wrote {} bytes and requested sync-pages", file_path, written);
    return 0;
}
