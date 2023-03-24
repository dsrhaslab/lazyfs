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
```

I recommend following the `simple` cache configuration (indicating the cache size and using a similar configuration file as `default.toml`), since it's currently the most tested schema in our experiments. Additionally, for the section **[cache]**, you can specify the following:

-   **apply_eviction**: Whether the cache should behave like the real page cache, evicting pages when the cache fills to the maximum.

-   **[cache.simple]** or **[cache.manual]**: To setup the cache size and internal organization. For now, you could just follow the example using the **custom_size** in (Gb/Mb/Kb) and the **number of blocks in each page** (you can just leave 1 as default). For manual configurations, comment out the simple configuration and uncoment/change the example above to suit your needs.

Other parameters:

- **fifo_path**: The absolute path where the faults FIFO should be created.
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

LazyFS expects that every buffer written to the FIFO file terminates with a new line character. Thus, if using `pwrite`, for example, make sure you end the buffer with `\n`.

## Contact

For additional information regarding issues, possible improvements and collaborations please contact:

- **João Azevedo** - [joao.azevedo@inesctec.pt](mailto:joao.azevedo@inesctec.pt)
