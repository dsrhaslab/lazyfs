# Reproduced Bugs
All bugs reproduced with LazyFS can be found here.

## Build images
To **build** local images that use LazyFS to reproduce bugs, run the `tests/build.sh` script. Each generated image is designed to reproduce a specific bug and is named with a unique identifier, the name of the System Under Test (SUT), and, if applicable, the version of the SUT affected by the bug. The test scripts for these bugs are organized by SUT and can be found in subdirectories within this directory.

Run `tests/run-local.sh` to **run** those local images.

## Pull images
Alternatively, it's possible to pull from Docker Hub the images that the script `tests/build.sh` produces. 

Run `tests/run-hub.sh` to **pull** and **run** those images.

## Jepsen test

The `jepsen` directory includes steps for running a Jepsen test that uses LazyFS to inject faults. There is a README that explains how to run the test.
