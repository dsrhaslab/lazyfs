
# Benchmarking tests

Benchmarking results from multiple micro- and macro-workloads of the File system and storage benchmark [Filebench](https://github.com/filebench/filebench).

## Tests description

- Each workload runs for **5 minutes**, using **1 thread** and an IO **block size of 4k**;
- We **compare** the results with the **FUSE passthrough** program;
- In LazyFS, we **vary the page size** (4k and 32k) which generally increases the throughput of read/write operations;
- In both systems we ran all workloads with and without the **direct_io** flag set, clearing OS and LazyFS caches before every run in case of **direct_io=1**.

## Scripts

- **results.sh**: Generates the results folder based on a template folder

    ```console
    ./results.sh -w workload_name -o output_folder
    ```

- **setup-testbed.sh**: Creates workloads dirs and overrides the workload path in filebench test files:

    ```console
    ./setup-testbed.sh -d fs_workload_dir -f workload_file.f
    ```

- **summary.sh**: Generates a pretty-printed table with the results:

    ```console
    ./summary -r results_folder
    ```

- **run-workloads.sh**: Runs a list of workload names from the workloads folder, also creates all the results for any system (passthrough or LazyFS)

    ```console
    ./run-workloads.sh
    ```

## Workloads

We run sequential and randomized versions of read and write workloads. Also there are macro workloads that mix these operations and increase the work to be done. Finally, some metadata intensive tests were also performed.