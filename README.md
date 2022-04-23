# LazyFS

A FUSE Filesystem with its own configurable page cache which can be used to simulate failures on un-fsynced data.

## Supported operations

Our filesystem implements several system calls that query our custom page cache layer. Currently, **LazyFS** supports the most important filesystem operations, e.g. **read**/**write**/**truncate**/**fsync**, but it is <u>**not yet fully tested and optimized**</u>.

## Installation

LazyFS was tested with **ext4** (default mount options) as the underlying filesystem in both Debian 11 (bullseye) and Ubuntu 20.04 (focal) environment. It is C++14 compliant and requires the following packages to be installed:

-   **CMake** (>= 3.10) and **g++** (>= 9.4.0):

```bash
sudo apt install g++ cmake
```

-   **FUSE** 3


```bash
sudo apt install libfuse3-dev libfuse3-3 fuse3
```

The filesystem requires the option **'allow_other'** as a startup argument so that other users can read and write files, besides the user owning the filesystem. For that, you must uncomment/add the following line on the configuration file **/etc/fuse.conf**:

```bash
user_allow_other
```

Secondly, you must compile and install the caching library **libpcache**, which will be attached to the **LazyFS**:

```bash
cd libs/libpcache && ./build.sh && cd -
```

Finally, build the **LazyFS** with the following command:

```bash
cd lazyfs && ./build.sh && cd -
```

## Running and Injecting faults

**LazyFS** uses a toml configuration file to set up the cache and a named pipe to append fault commands:

```bash
# lazyfs/config/default.toml

[faults]
fifo_path="/my/path/faults-example.fifo"

[cache]
apply_eviction=false

[cache.simple]
custom_size="0.5GB"
blocks_per_page=1
# [cache.manual]
# io_block_size=4096
# page_size=4096
# no_pages=10
```

The section **[cache]** **requires** that you specify the following:

-   **apply_eviction**: Whether the cache should behave like the real page cache, evicting pages when the cache fills to the maximum;

-   **[cache.simple]** or **[cache.manual]**: To setup the cache size and internal organization. For now, one could just specify as seen in the example, the **custom_size** in (Gb/Mb/Kb) and the **number of blocks in each page** (you can just leave 1 as default). For manual configurations, comment out the simple configuration and uncoment/change the example above to suit your needs.

To **run the filesystem**, one could use the **mount-lazyfs.sh** script, which calls FUSE with the correct parameters:

```bash
cd lazyfs/

# Running LazyFS in the foreground (add '-f/--foregound')

./scripts/mount-lazyfs.sh -c config/default.toml -m /mnt/lazyfs -r /mnt/lazyfs-root -f

# Running LazyFS in the background

./scripts/mount-lazyfs.sh -c config/default.toml -m /mnt/lazyfs -r /mnt/lazyfs-root

# Umount with

./scripts/umount-lazyfs.sh -m /mnt/lazyfs

# Display help

./scripts/mount-lazyfs.sh --help
./scripts/umount-lazyfs.sh --help
```

Finally, to inject faults one could append/echo the clear cache command to the fifo path specified above:

```bash
echo "lazyfs::clear-cache" > /my/path/faults-example.fifo
```

To display the current cache usage:

```bash
echo "lazyfs::display-cache-usage" > /my/path/faults-example.fifo
```

## Development

This section displays the next tasks and some documentation resources:

### TODO

-   [x] Add **truncate** operation to the page cache **(in tests)**
-   [x] Add benchmarking tests (w/ Filebench) **(in tests)**
    - Located at **test/filebench/general-results.table**
-   [ ] Performance optimizations **(current)**
-   [ ] Add integrity tests
-   [ ] Test with more DBs besides PostgreSQL
-   [ ] Make **rename** operations atomic
-   [ ] Configure cache to grow until a certain size (dynamically)
-   [ ] Add logging to an external file
-   [ ] Code documentation

### Resources

-   Access/Modify/Change times: https://stackoverflow.com/questions/3385203/what-is-the-access-time-in-unix
-   FUSE Cheatsheet: https://www.cs.hmc.edu/~geoff/classes/hmc.cs137.201801/homework/fuse/fuse_doc.html
