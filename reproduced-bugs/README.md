# Reproduced Bugs
All bugs reproduced with LazyFS can be found here. You also can find here some tests for validating crash consistency mechanisms. 

## Run all tests
Execute the following commands to run all the tests:

```shell
cd tests
chmod +x ./run-hub.sh
./run-hub.sh
```

This script **pulls** images from Docker Hub and **runs** those images.

After executing this script, it is expected to have 23 images and 29 containers. 

Each container corresponds to a different test. While running, the current state of the test is outputted (e.g., LazyFS is starting, a fault was injected) along with information on whether the error or impact on the System Under Test (SUT) is as expected. If you see "Error expected detected", the bug was successfully reproduced. 


## Build and run images
Additionally, if you desire to build locally these images, execute the following commands:

```shell
chmod +x build.sh
./build.sh
```

Next, execute the following commands to run those images (still inside the `tests` directory):

```shell
chmod +x ./run-local.sh
./run-local.sh
```


## Jepsen test

The `jepsen` directory includes steps for running a Jepsen test that uses LazyFS to inject faults. There is a README that explains how to run the test.

## Reproduce manually 
If you do not want to use Docker to reproduce the bugs, you can run yourself each bug. 

The scripts for each test are organized into directories by SUT. Inside each SUT directory, you will find a directory for each bug and crash consistency validation test. For instance, if we want run bug 6 of LevelDB, we should go to the directory `leveldb/leveldb-6`. In this directory, we will encounter the main script for this test, named `leveldb-6.sh`, along with other files that contain workloads and additional information. You will probably need to change the script that ends with `var.sh` since this contains important paths for the experiments. 

Additionally, inside each SUT folder you will find a script for installing the SUT. In some cases, it is necessary to provide a version as argument for this script. You can find the version in the header of the desired script file of the bug or crash consistency validation test script. 

Certain test scripts also require arguments. Read the header of the desired script to understand what is needed.

In summary, follow these steps to reproduce manually the bugs:
1. Follow the instructions to install LazyFS.
2. Install the SUT (e.g., LevelDB, ZooKeeper). 
3. Execute the script of the corresponding test.