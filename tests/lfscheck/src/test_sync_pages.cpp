/*
 * Simple single-threaded test to exercise lazyfs "sync-pages" faults.
 *
 * The test will create the file under the mount point, write multiple pages
 * of data without fsync, then send a sync-pages command through the FIFO
 * to instruct LazyFS to flush pages for the file.
 */

#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include "argparse/argparse.hpp"
#include "cstring"

using namespace std;


// Test specification

const size_t PAGE_SIZE = 4096;
bool single_threaded   = false;
string mount           = "";
string fifo            = "";
string rel_path        = "";      


enum class Pages {
    ALL, // Sync all pages
    FIRST_HALF, // Sync first half of the pages
    SECOND_HALF, // Sync second half of the pages
    FIRST, // Sync first page
    LAST, // Sync last page
    FIRST_AND_LAST, // Sync first and last pages
    INTERLEAVED, // Sync interleaved pages
    RANDOM // Sync random pages
};

// ---------------------------------------

Pages parse_pages_option(const string& option) {
    if (option == "all") return Pages::ALL;
    if (option == "first-half") return Pages::FIRST_HALF;
    if (option == "second-half") return Pages::SECOND_HALF;
    if (option == "first") return Pages::FIRST;
    if (option == "last") return Pages::LAST;
    if (option == "first-and-last") return Pages::FIRST_AND_LAST;
    if (option == "interleaved") return Pages::INTERLEAVED;
    if (option == "random") return Pages::RANDOM;

    throw invalid_argument("Invalid pages option: " + option);
}

/** 
 * Writes 'count' bytes of character 'c' to file descriptor 'fd'.
 * @param fd File descriptor to write to.
 * @param c Character to write.
 * @return Number of bytes written, or -1 on error.
 */
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

/**
 * Writes a sequence of characters to the file descriptor, cycling from 'A' to 'Z'. Writes PAGE_SIZE bytes of a character at a time. 
 * @param fd File descriptor to write to.
 * @param buffer Buffer what was written to the file. Simulates expected file contents.
 * @param total_size Total number of bytes to write.
 * @return Number of bytes written, or -1 on error.
 */
int write_chars(int fd, char* buffer, size_t total_size) {
    size_t written = 0;
    char current_char = 'A';

    while (written < total_size) {
        size_t to_write = std::min(PAGE_SIZE, total_size - written);
        ssize_t w = write_char(fd, current_char, to_write);
        if (w < 0) {
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


bool compare_buffers(char* expected, char* real, size_t total_size, Pages fault) {
    switch (fault) {
        case Pages::ALL:
            // The contents of the entire buffers should match, because all pages were synced
            return memcmp(expected, real, total_size) == 0;
        case Pages::FIRST_HALF:
            // The first half should match
            return memcmp(expected, real, total_size / 2) == 0;
        case Pages::SECOND_HALF:
            return memcmp(expected + total_size / 2, real + total_size / 2, total_size - total_size / 2) == 0;
        case Pages::FIRST:
            return memcmp(expected, real, PAGE_SIZE) == 0;
        case Pages::LAST:
            return memcmp(expected + total_size - PAGE_SIZE, real + total_size - PAGE_SIZE, PAGE_SIZE) == 0;
        case Pages::FIRST_AND_LAST:
            return memcmp(expected, real, PAGE_SIZE) == 0 &&
                   memcmp(expected + total_size - PAGE_SIZE, real + total_size - PAGE_SIZE, PAGE_SIZE) == 0;
        case Pages::INTERLEAVED: {
            for (size_t offset = 0; offset < total_size; offset += 2 * PAGE_SIZE) {
                size_t len = std::min(PAGE_SIZE, total_size - offset);
                if (memcmp(expected + offset, real + offset, len) != 0) {
                    return false;
                }
            }
            return true;
        }
        default:
            return false;
    }

    return false;
} 

bool check_file_contents(const string& path, char* expected_buffer, size_t total_size, Pages fault) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open for read");
        close(fd);
        return false;
    }

    char read_buffer[total_size];
    ssize_t r = read(fd, read_buffer, total_size);
    if (r < 0) {
        perror("read error for file contents check");
        close(fd);
        return false;
    }

    close(fd);
    return compare_buffers(expected_buffer, read_buffer, total_size, fault);

}




int main(int argc, char** argv) {
    argparse::ArgumentParser ap("test_sync_pages");

    ap.add_argument("-f", "--fifo").required().help("lazyfs FIFO path");
    ap.add_argument("-m", "--mount").required().help("lazyfs mount point");
    ap.add_argument("-s", "--single-threaded").default_value("true").help("run in single-threaded mode");
    ap.add_argument("-p", "--path").required().help("relative path of test file to create");
    ap.add_argument("-z", "--size").required().help("total size of test file in bytes");
    ap.add_argument("-fi", "--fault-injection").required().help("type of fault to inject for sync-pages (all, first-half, second-half, first, last, first-and-last, interleaved, random)");
    ap.parse_args(argc, argv);

    try {
        ap.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        cerr << err.what() << endl;
        cerr << ap;
        return 1;
    }

    fifo = ap.get<string>("--fifo");
    mount = ap.get<string>("--mount");
    rel_path = ap.get<string>("--path");
    size_t total_size = ap.get<int>("--size");
    string fault_injection = ap.get<string>("--fault-injection");

    Pages pages_option = parse_pages_option(fault_injection);


    if (fifo.empty() || mount.empty() || rel_path.empty() || total_size <= 0) {
        cerr << "Missing required arguments or invalid size.\n";
        cerr << "Usage: " << argv[0] << " -f <fifo> -m <mount> -p <path> -z <size>\n";
        return 1;
    }
    
    string file_path = filesystem::absolute(mount + "/" + rel_path).string();

    std::cout << "creating test file: " << file_path << " (" << total_size << " bytes)" << std::endl;

    int fd = open(file_path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open create: could not create test file");
        return 1;
    }

    char* expected_buffer = new char[total_size];
    int written = write_chars(fd, expected_buffer, total_size);
    if (written < 0) {
        std::cerr << "error writing test file" << std::endl;
        close(fd);
        return 1;
    }

    // Construct sync-pages command: 
    // timing=now, pages=first-and-last, sync-other-files=true
    string command = "lazyfs::sync-pages::timing=now::pages=" + fault_injection + "::file=" + file_path + "::sync-other-files=true\n";

    std::cout << "sending command to FIFO " << fifo << ": " << command << std::endl;

    int fd_fifo = open(fifo.c_str(), O_WRONLY);
    if (fd_fifo < 0) {
        perror("open fifo");
        return 1;
    }

    ssize_t rc = write(fd_fifo, command.c_str(), command.size());
    if (rc < 0) perror("write fifo");
    close(fd_fifo);

    std::cout << "command sent, waiting 3s for LazyFS to process..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "test finished: file=" << file_path << " wrote " << written << " bytes and requested sync-pages" << std::endl;

    close(fd);

    bool res = check_file_contents(file_path, expected_buffer, total_size, pages_option);

    cout << "file contents " << (res ? "match expected data after sync-pages" : "DO NOT match expected data after sync-pages") << std::endl;

    return 0;
}
