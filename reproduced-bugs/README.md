# Tests
Run `tests/build.sh` to build images with the tests. Each image includes a bug identification (a number), the name of the SUT and, in cases where the bug is seen across different versions, it also includes the version of the system. 

Run `tests/run.sh` to run those images.

The `jepsen` directory includes steps for running a Jepsen test that uses LazyFS to inject faults. There is a README that explains how to run the test.
