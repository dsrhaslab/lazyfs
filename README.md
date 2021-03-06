# LazyFS

<h1 align="center">
    <img src=".media/LazyFS.png" width="60%">
    <br>
    <img src="https://img.shields.io/badge/C++-17-yellow.svg?style=flat&logo=c%2B%2B" />
    <img src="https://img.shields.io/badge/status-research%20prototype-green.svg" />
    <a href="https://opensource.org/licenses/BSD-3-Clause">
        <img src="https://img.shields.io/badge/License-MIT-blue.svg" />
    </a>
</h1>

A FUSE Filesystem with its own configurable page cache which can be used to simulate failures on un-fsynced data.

## Supported operations

This filesystem implements several system calls that query a custom page cache layer. Currently, **LazyFS** supports the most important filesystem operations, e.g. **read**/**write**/**truncate**/**fsync**, but it is <u>**not yet fully tested and optimized**</u>.

## Installation

LazyFS was tested with **ext4** (default mount options) as the underlying filesystem in both Debian 11 (bullseye) and Ubuntu 20.04 (focal) environment. It is C++17 compliant and requires the following packages to be installed:

-   **CMake** (>= 3.10) and **g++** (>= 9.4.0):

```bash
sudo apt install g++ cmake
```

-   **FUSE** 3

```bash
sudo apt install libfuse3-dev libfuse3-3 fuse3
```

It requires the option **'allow_other'** as a startup argument so that other users can read and write files, besides the user owning the filesystem. For that, you must uncomment/add the following line on the configuration file **/etc/fuse.conf**:

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

[filesystem]
sync_after_rename=false # default option
log_all_operations=false # default option
```

The section **[cache]** **requires** that you specify the following:

-   **apply_eviction**: Whether the cache should behave like the real page cache, evicting pages when the cache fills to the maximum;

-   **[cache.simple]** or **[cache.manual]**: To setup the cache size and internal organization. For now, one could just specify as seen in the example, the **custom_size** in (Gb/Mb/Kb) and the **number of blocks in each page** (you can just leave 1 as default). For manual configurations, comment out the simple configuration and uncoment/change the example above to suit your needs.

Other available parameters:

- faults.**fifo_path**: The absolute path where the faults pipe should be created;
- filesystem.**sync_after_rename**: Whether to implicitly perform a sync after a rename operation;
- filesystem.**log_all_operations**: Just experimental for quick debugging in stdout. The LazyFS roadmap will include better logging in the future.


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

Finally, one can control LazyFS by echoing the following commands to the faults fifo file:

-   **Clear cache,** this command clears any un-fsynced data:

    ```bash
    echo "lazyfs::clear-cache" > /my/path/faults-example.fifo
    ```

-   **Checkpoint,** which checkpoints any un-fsynced data by calling write to the underlying filesystem:

    ```bash
    echo "lazyfs::cache-checkpoint" > /my/path/faults-example.fifo
    ```

-   **Show usage,** which displays the current cache usage:

    ```bash
    echo "lazyfs::display-cache-usage" > /my/path/faults-example.fifo
    ```

> **Notice**: LazyFS expects that every buffer written to the faults fifo file terminates with a new line character (**echo** does this by default). Otherwise, you will probably get a log like this: "[command]: unknown <lazyfs::clear-cach>".

## Development

This section displays a table with the current work planned:

| **Tasks**                                    	| **Status**  	|
|----------------------------------------------	|-------------	|
| Performance optimizations (e.g. using perf)  	| In progress 	|
| Develop Filesystem unit tests                	| In progress 	|
| Test with more applications / databases      	| In progress 	|
| Ensure rename atomicity                      	|      -      	|
| Configure the page cache to grow dynamically 	|      -      	|
| Improve logging (log formats and logfile)    	|      -      	|

## Contact

For additional information regarding issues, possible improvements and collaborations please contact:

- **Jo??o Azevedo** - [joao.azevedo@inesctec.pt](mailto:joao.azevedo@inesctec.pt)