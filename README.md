# LazyFS

A FUSE Filesystem with its own configurable page cache which can be used to simulate failures on un-fsynced data.

## Supported operations

Our filesystem implements severall system calls that query our custom page cache layer. Currently, **LazyFS** supports most of the operations except **truncate** (in development) and it is **not yet fully optimized**.

## Installation

LazyFS requires the following packages to be installed:

-   **CMake** and **g++**:
    -   sudo apt install g++ cmake
-   **FUSE** 3

Install and configure FUSE (version 3):

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

**LazyFS** uses a toml configuration file to setup the cache and a named pipe to append fault commands:

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

-   **apply_eviction**: Wether the cache should behave like the real page cache, evicting pages when the cache fills to the maximum;

-   **[cache.simple]** or **[cache.manual]**: To setup the cache size and internal organization. For now, one could just specify as seen in the example, the **custom_size** in (Gb/Mb/Kb) and the **number of blocks in each page** (you can just leave 1 as default). For manual configurations, comment out the simple configuration and uncoment/change the example above to suit your needs.

To **run the filesystem**, one could use the **mount-lazyfs.sh** script, which calls FUSE with the correct parameters:

```bash
# Edit this script to change the MOUNT_DIR and the ROOT_DIR
#
# Defaults to:
#       MOUNT_DIR=/tmp/lazyfs     (FUSE mount point)
#       ROOT_DIR=/tmp/lazyfs-root (FUSE subdir: 'prepends this directory to all paths')

# Running the filesystem in foreground mode...

cd lazyfs/

./scripts/mount-lazyfs.sh -c config/default.toml --foreground

# Display help

./scripts/mount-lazyfs.sh --help

# Umount with

./scripts/umount-lazyfs.sh
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

-   [ ] Add **truncate** operation to the page cache
-   [ ] Add integrity tests
-   [ ] Add benchmarking tests (w/ Filebench)
-   [ ] Performance optimizations
-   [ ] Test with more DBs besides PostgreSQL
-   [ ] Make **rename** operations atomic
-   [ ] Configure cache to grow until a certain size (dynamically)
-   [ ] Add logging to an external file

### Resources

-   Access/Modify/Change times: https://stackoverflow.com/questions/3385203/what-is-the-access-time-in-unix
-   FUSE Cheatsheet: https://www.cs.hmc.edu/~geoff/classes/hmc.cs137.201801/homework/fuse/fuse_doc.html
