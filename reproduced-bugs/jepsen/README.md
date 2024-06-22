# Jepsen test

Script `jepsen-setup.sh` installs Jepsen, changes the necessary configurations and sets up a cluster of 5 nodes for running the test.
  
Script `jepsen-control-node.sh` includes the steps to be executed inside the control node (the node that will control the test).

**Disclaimer**: This bug is not always reproducible. 

### Note
This test will require some space from disk, so make sure you have enough space available.

