
# LazyFS Tests

LazyFS is currently being tested in different ways to ensure it is doing what is expected, although we are constantly looking for new bugs. The storage community today tends to focus more on benchmarking performance over correctness and we lack a well known tool to do our tests.

For now, these are the **test suites and scripts** you can use to check LazyFS's correctness:

1. **Unit**

    Covers the basic functionality of LazyFS with deterministic testing. Allows keeping track of edge cases that caused bugs in the past.

2. **lfscheck**

    This tool was built to ensure that after clearing the cache, LazyFS shows only the synced data since the last *fsync* call. The test is composed of N worker threads and one monitor thread:

    - Each **Worker** thread uses an independent file (e.g. `file_worker_id`) and does the following sequence of commands repeatedly:
        
        - **write** random data to random offsets
        - **read** last written data to ensure it was written correctly
        - **sync** sometimes (randomly)

    - The **Monitor** thread issues `lazyfs::clear-cache` commands each S seconds, ensuring that all worker threads are waiting for the clear cache command to finish;


    After each clear cache command, each worker reads the data from the filesystem and compares it with an in-memory buffer with only the synced data.

    To run `lfscheck`, one can configure the number of concurrent files accessed by specifying the number of threads and the minimum time the test will run (thread syncing and locking could delay the total time).

    Example usage:

    ```console
    # Build lfscheck
    cd tests/lfscheck
    ./build.sh

    # Mount LazyFS
    # ./scripts/mount-lazyfs ...

    # Run example
    ./build/lfscheck -m MOUNT_DIR -f FIFO_PATH -n NR_THREADS -t TEST_TIME_SECS
    ./build/lfscheck --help
    ```

    There is an example output (`examples/example-100th-60s`) of running `lfscheck` for 100 threads (100 files), during a one minute run.

    With this test we are able to create multiple scenarios where different files are in completely different states (different contents, un-fsynced data, etc.)

3. Jepsen tests for local filesystems ([jepsen-io/local-fs](https://github.com/jepsen-io/local-fs))

    This tool allows generating random histories of filesystem operations (through shell-based utilities like `echo`, `rm`, `mkdir`, `truncate`, `ln` etc.), applying them to a real filesystem (e.g. `ext4`). It then checks to see whether LazyFS is behaving correctly by comparing outputs, in a purely-funcional model (Read more about `local-fs` [here](https://github.com/jepsen-io/local-fs/blob/main/README.md)).

    To test a version of LazyFS, one can specify the commit hash and pass it to the quickcheck test:

    ```console
    lein run quickcheck --db lazyfs --version LAZYFS_COMMIT_HASH
    ```
    
    > You may need to install some dependencies (e.g. `leiningen` and LazyFS's dependencies).

I am always looking for new ways of testing this system, please feel free to send me suggestions!