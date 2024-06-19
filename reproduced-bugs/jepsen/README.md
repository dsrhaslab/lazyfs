# Jepsen Bug #20
**Disclaimer**: This bug is not always reproducible. 

Script `jepsen-setup.sh` installs Jepsen, changes the necessary configurations and sets up a cluster of 5 nodes for running the test.
  
Script `jepsen-control-node.sh` includes the steps to be executed inside the control node (the node that will control the testing).