
# Sample testing programs

A collection of simple programs to test the basic functionalities of a filesystem.

```console
# Compile all examples
make

# Run
./read <size> <offset> <path>
./write <size> <offset> <char> <path>
./truncate <size> <path>
./fsync <mode=all,data> <path>
```

The preallocation regression checks real backing extents, unchanged logical
file size, argument errors, and preservation of unsynced cache contents.
Use a fresh file and its corresponding backing path:

```sh
./preallocate prepare /mount/test-file /backing/test-file
# Inject lazyfs::clear-cache and wait for its finished::clear-cache reply.
./preallocate verify /mount/test-file /backing/test-file
```

Only `FALLOC_FL_KEEP_SIZE` is supported. Other allocation modes fail rather
than change backing contents while bypassing the file cache.

`bash shutdown.sh /path/to/lazyfs` checks clean unmount in four cases: no
completion FIFO, an unopened completion FIFO, repeated acknowledged
cache-loss commands, and a cache-loss command awaiting a missing listener.
It requires FUSE mount access and fails on a blocked fault worker or an
abnormal filesystem exit. Set `TMPDIR` to choose its artifact directory.
