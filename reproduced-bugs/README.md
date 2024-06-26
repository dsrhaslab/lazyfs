# Reproduced Bugs
All bugs reproduced with LazyFS can be found here. You also can find here some tests for validating crash consistency mechanisms. 

## Run all tests
Execute the following commands to run all the tests:

```shell
chmod +x tests/run-hub.sh
tests/run-hub.sh
```

This script **pulls** images from Docker Hub and **runs** those images.

After executing this script, it is expected to have 22 images and 26 containers. 

Each container corresponds to a different bug. While running, the current state of the test is outputted (e.g., LazyFS is starting, a fault was injected) along with information on whether the error or impact on the System Under Test (SUT) is as expected. If you see "Error expected detected", the bug was successfully reproduced. 


## Build and run images
Additionally, if you desire to build locally these images, execute the following commands:

```shell
chmod +x tests/build.sh
tests/build.sh
```

Next, execute the following commands to run those images:

```shell
chmod +x tests/run-local.sh
tests/run-local.sh
```


## Jepsen test

The `jepsen` directory includes steps for running a Jepsen test that uses LazyFS to inject faults. There is a README that explains how to run the test.

## Reproduce manually 
If you do not want to use Docker to reproduce the bugs, you can run yourself each bug. 

The scripts for each test are organized into directories by SUT. Inside each SUT directory, you will find a directory for each bug, which has an unique name "[SUT]-[unique identifier]" or "[SUT]-vcc", when the test corresponds to a crash consistency validation test. For instance, if we want run bug 6 of LevelDB, we should go to the directory `leveldb/leveldb-6`. In this directory, we will encounter the main script for this test, named `leveldb-6.sh`, along with other files that contain workloads and additional information. 

Additionally, inside each SUT folder you will find a script for installing the SUT. In some cases, it is necessary to provide a version. You can find the version in the header of the script file of the bug.

In summary, follow these steps to reproduce manually the bugs:
1. Follow the instructions to install LazyFS.
2. Install the SUT (i.e., LevelDB, ZooKeeper). 
3. Execute the script of the corresponding bug.