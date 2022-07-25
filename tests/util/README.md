
# Sample testing programs

A collection of simple programs to test the basic functionalities of a filesystem.

```console
# Compile all examples
make

# Run
./read <size> <offset> <path>
./write <size> <offset> <char> <path>
./truncate <size> <path>
./fsync <mode=all,data> <path>
```