# LazyFS

<h1 align="center">
    <img src=".media/LazyFS.png" width="60%">
    <br>
    <img src="https://img.shields.io/badge/C++-17-yellow.svg?style=flat&logo=c%2B%2B" />
    <a href="https://github.com/dsrhaslab/lazyfs/actions/workflows/build.yaml">
        <img src="https://github.com/dsrhaslab/lazyfs/actions/workflows/build.yaml/badge.svg" />
    </a>
    <img src="https://img.shields.io/badge/status-research%20prototype-green.svg" />
    <a href="https://opensource.org/licenses/BSD-3-Clause">
        <img src="https://img.shields.io/badge/License-MIT-blue.svg" />
    </a>
</h1>

A FUSE file system with an internal dedicated page cache that only flushes data if explicitly requested by the application. This is useful for simulating power failures and losing all unsynced data.

> **Note**: The main branch is probably unstable and with not fully-tested features, so we recommend using one of [existing releases](https://github.com/dsrhaslab/lazyfs/releases) already created.

## Installation

LazyFS was tested with **ext4** (default mount options) as the underlying file system (FUSE backend), in both Debian 11 (bullseye) and Ubuntu 20.04 (focal) environment. It is C++17 compliant and requires the following packages to be installed:

-   `CMake` and `g++`:

```bash
sudo apt install g++ cmake

# The following versions are used during development:
# cmake: 3.16.3
# g++: 9.4.0
```

-   `FUSE 3`:

```bash
sudo apt install libfuse3-dev libfuse3-3 fuse3
```

FUSE requires the option `allow_other` as a startup argument so that other users can read and write files, besides the user owning the file system. For that, you must uncomment/add the following line on the configuration file `/etc/fuse.conf`:

```bash
user_allow_other
```

Compile and install the caching library `libpcache`, which will be attached to **LazyFS**:

```bash
cd libs/libpcache && ./build.sh && cd -
```

Finally, build `lazyfs`:

```bash
cd lazyfs && ./build.sh && cd -
```

## Running and Injecting faults

**LazyFS** uses a [toml](https://toml.io/en/) configuration file to set up the cache and a named pipe to append fault commands:

```bash
# file: lazyfs/config/default.toml

[faults]
fifo_path="/tmp/faults.fifo"
# fifo_path_completed="/tmp/faults_completed.fifo"

[cache]
apply_eviction=false

[cache.simple]
custom_size="0.5GB"
blocks_per_page=1

# [cache.manual]
# io_block_size=4096
# page_size=4096
# no_pages=10

[file system]
log_all_operations=false
logfile="/tmp/lazyfs.log"

[[injection]]
type="reorder"
op="write"
file="output.txt"
persist=[1,4]
ocurrence=2

[[injection]]
type="split_write"
file="output1.txt"
ocurrence=5
parts=3 #or parts_bytes=[4096,3600,1260]
persist=[1,3]
```

I recommend following the `simple` cache configuration (indicating the cache size and using a similar configuration file as `default.toml`), since it's currently the most tested schema in our experiments. Additionally, for the section **[cache]**, you can specify the following:

-   **apply_eviction**: Whether the cache should behave like the real page cache, evicting pages when the cache fills to the maximum.

-   **[cache.simple]** or **[cache.manual]**: To setup the cache size and internal organization. For now, you could just follow the example using the **custom_size** in (Gb/Mb/Kb) and the **number of blocks in each page** (you can just leave 1 as default). For manual configurations, comment out the simple configuration and uncoment/change the example above to suit your needs.

It is possible to specify a set of faults for injection. The example above illustrates how to define the two currently available fault types that can be defined through the configuration file (other faults are injected while LazyFS is running). For each injection, it is mandatory to specify the type, which can be one of the following:

-   **reorder**: This fault type is used when a sequence of system calls involving a given file, is executed consecutively without an intervening fsync. In the example, during the second group of consecutive writes (the group number is defined by the parameter `occurrence`) to the file "output.txt", the first and fourth writes will be persisted to disk (the writes to be persisted are defined by the parameter `persist`). After the fourth write (the last in the `persist` vector), LazyFS will crash.
-   **split_write**: This fault type involves dividing a write system call into smaller portions, with some of these portions being persisted while others are not. In the example, the fifth write issued (the number of the write is defined by the parameter `occurrence`) to the file "output1.txt" will be divided into three equal parts if the `parts` parameter is used, or into customizable-sized parts if the `parts_bytes` parameter is defined. In the commented code, there's an example of using `parts_bytes`, where the write will be split into three parts: the first with 4096 bytes, the second with 3600 bytes, and the last with 1200 bytes. The `persist` vector determines which parts will be persisted. After the persistence of these parts, LazyFS will crash.
  
Other parameters:

- **fifo_path**: The absolute path where the faults FIFO should be created.
- **fifo_path_completed**: If we plan to inject the clear cache fault synchronously, it is necessary to determine the completion of the `lazyfs::clear-cache` command execution. By specifying this parameter, a message will be written to another FIFO (`finished::clear-cache`), so that users can set up a reader process that waits before making any post-fault consistency checks.
- **log_all_operations**: Whether to log all file system operations that LazyFS receives.
- **logfile**: The log file for LazyFS's outputs. Fault acknowledgment is sent to `stdout` or to the `logfile`.


To **run the file system**, one could use the **mount-lazyfs.sh** script, which calls FUSE with the correct parameters:

```bash
cd lazyfs/

# Running LazyFS in the foreground (add '-f/--foregound')

./scripts/mount-lazyfs.sh -c config/default.toml -m /tmp/lazyfs.mnt -r /tmp/lazyfs.root -f

# Running LazyFS in the background

./scripts/mount-lazyfs.sh -c config/default.toml -m /tmp/lazyfs.mnt -r /tmp/lazyfs.root

# Umount with

./scripts/umount-lazyfs.sh -m /tmp/lazyfs.mnt/

# Display help

./scripts/mount-lazyfs.sh --help
./scripts/umount-lazyfs.sh --help
```

Finally, one can control LazyFS by echoing the following commands to the configured FIFO:

-   **Clear cache** - clears all unsynced data:

    ```bash
    echo "lazyfs::clear-cache" > /tmp/faults.fifo
    ```

-   **Checkpoint** - checkpoints all unsynced data by calling `write` to the underlying file system (without `fsync`):

    ```bash
    echo "lazyfs::cache-checkpoint" > /tmp/faults.fifo
    ```

    > Note: Any subsequent failure is outside of the test control.

-   **Show usage** - displays the current cache usage (percentage of pages allocated):

    ```bash
    echo "lazyfs::display-cache-usage" > /tmp/faults.fifo
    ```

-   **Report unsynced data,** which displays the inodes that have data in cache:

    ```bash
    echo "lazyfs::unsynced-data-report" > /my/path/faults.fifo
    ```

-   **Kill the filesystem,** which is triggered by an operation, a timing and a path regex:

    Here **timing** should be one of `before` or `after`, and **op** should be a valid system call name (e.g. `write` or `read`).

    - In the case of operations that have a source path only (e.g. `create`, `open`, `read`, `write`, ...)

        ```bash
        echo "lazyfs::crash::timing=...::op=...::from_rgx=..." > /my/path/faults.fifo
        ```

        Here, `from_rgx` is required (do not specify to_rgx).

    - For `rename`, `link` and `symlink`, one is able to specify the destination path:

        ```bash
        echo "lazyfs::crash::timing=...::op=...::from_rgx=...::to_rgx=..." > /my/path/faults.fifo
        ```

        Here, only one of `from_rgx` or `to_rgx` is required.

    Example 1:

    ```bash
    echo "lazyfs::crash::timing=before::op=write::from_rgx=file1" > /my/path/faults.fifo
    ```

    > Kills LazyFS before executing a write operation to the file pattern 'file1'.

    Example 2:

    ```bash
    echo "lazyfs::crash::timing=before::op=link::from_rgx=file1::to_rgx=file2" > /my/path/faults.fifo
    ```

    > Kills LazyFS before executing a rename operation from the file pattern 'file1' to the file pattern 'file2'.

    Example 3:

    ```bash
    echo "lazyfs::crash::timing=before::op=rename::to_rgx=fileabd" > /my/path/faults.fifo
    ```

    > Kills LazyFS before executing a link operation to the file pattern 'fileabd'.

LazyFS expects that every buffer written to the FIFO file terminates with a new line character (**echo** does this by default). Thus, if using `pwrite`, for example, make sure you end the buffer with `\n`.

## Contact

For additional information regarding possible improvements and collaborations please open an issue or contact: [@devzizu](https://github.com/devzizu), [@mj-ramos](https://github.com/mj-ramos) and [@dsrhaslab](https://github.com/dsrhaslab).
