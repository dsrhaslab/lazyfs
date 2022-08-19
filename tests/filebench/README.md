
## Requirements

- Check if the following tools are installed:

    ```bash
    sudo apt install screen dstat
    ```

## Filebench

1. Install from the latest release [filebench-1.5-alpha3](https://github.com/filebench/filebench/releases)

    - Check if the following tools are installed:

        ```bash
        bison
        flex
        libtoolize
        aclocal
        autoheader
        automake --add-missing # must be 1.15 (sudo apt install automake-1.15)
        autoconf
        ```

    - Compile and install:

        ```bash
        cd filebench-1.5-alpha3
        ./configure
        make
        sudo make install
        ```

    - Disable ASLR (Address Space Layout Randomization):

        ```bash
        sudo -s
        root> echo 0 > /proc/sys/kernel/randomize_va_space
        ```

    - Change the workload main variables in `workloads/run-workloads.sh`:

        ```bash
        # Main required variables

        TEST_VAR_WORKLOADS_FOLDER=workloads/test
        TEST_VAR_OUTPUT_RESULTS_FOLDER=outputs

        TEST_VAR_TOTAL_RUNTIME_EACH_WORKLOAD=900 # seconds
        TEST_VAR_REPEAT_WORKLOADS=3 # number of times to re-run workloads
        TEST_VAR_FOR_FILESYSTEMS=("fuse.lazyfs-4096") # options: 'fuse.passthrough' and 'fuse.lazyfs-pagesize'
        # TEST_VAR_FOR_FILESYSTEMS=("fuse.lazyfs-4096" "fuse.passthrough" "fuse.lazyfs-32768")
        ```

    > Make sure you have the passthrough example from fuse compiled and the full path to the binary should be specified in this script (set **PASST_BIN**). The same goes for the LazyFS installation path.

## Results

1. Install dependencies:

    ```bash
    sudo apt install python3 python3-pip
    pip3 install prettytable pandas pip3 install matplotlib
    ```

2. Generate the results table:

    ```bash
    ./utils/gentable.py <OUTPUT_FOLDER_PATH>
    # Example: ./utils/dstatgraph.py outputs
    ```

3. Generate the results dstat graphs:

    ```bash
    ./utils/dstatgraph.py <OUTPUT_FOLDER_PATH> <WORKLOAD_NAME>
    # Example: ./utils/dstatgraph.py outputs varmail
    ```

## Notes


- If `dstat` reports the following error:

    ```bash
    Traceback (most recent call last):
      File "/usr/bin/dstat", line 2847, in <module>
        main()
      File "/usr/bin/dstat", line 2687, in main
        scheduler.run()
      File "/usr/lib/python3.7/sched.py", line 151, in run
        action(*argument, **kwargs)
      File "/usr/bin/dstat", line 2806, in perform
        oline = oline + o.showcsv() + o.showcsvend(totlist, vislist)
      File "/usr/bin/dstat", line 547, in showcsv
        if isinstance(self.val[name], types.ListType) or isinstance(self.val[name], types.TupleType):
    NameError: name 'types' is not defined
    ```

    Fix the script using this [StackExchange](https://serverfault.com/questions/996996/dstat-fails-to-start-trying-to-load-python3s-csv) alternative.