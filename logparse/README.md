# LazyFS Output Parser

LazyFS produces a log file when the option `log_all_operations` is set to true and outputs to a path when the option `logfile` is given. 
To simplify the visualization of the log, this parser can be used. It allows to:

- Filter specific system calls
- Filter system calls for a specific path
- Create a visualization graph from the log
- Group system calls, *i.e.*, if the same syscalls are called sequentially, the program identifies this repetition and simplifies the output
- Obtain information of the location of writes bigger than a page (this value is also configurable)
  
An example of output:

![](graph.png)
  
To understand the different options run: 

```bash
./parse.py -h
```