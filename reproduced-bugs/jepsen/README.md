# Jepsen test

Script `jepsen-setup.sh` installs Jepsen, changes the necessary configurations and sets up a cluster of 5 nodes for running the test.
  
Script `jepsen-control-node.sh` includes the steps to be executed inside the control node (the node that will control the test).

Execute the following commands to run this test:

```shell
chmod +x jepsen-setup.sh
./jepsen-setup.sh
```

When each node displays the message "Debian GNU/Linux 11 console", execute the following command to copy the `jepsen-control-node.sh` and enter the control node:

```shell
docker cp jepsen-control-node.sh jepsen-control:/jepsen/jepsen-control-node.sh
docker exec -it jepsen-control bash
```

Once you are inside the control node container, execute the following commands:

```shell
chmod +x jepsen-control-node.sh
./jepsen-control-node.sh
```


**Disclaimer**: This bug is not deterministic. It may take more than one run to find it. 

### Note
This test will require some space from disk, so make sure you have enough space available.

