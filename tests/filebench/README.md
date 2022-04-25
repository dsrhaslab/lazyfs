
# Benchmarking tests

Benchmarking results from multiple micro- and macro-workloads of the File system and storage benchmark [Filebench](https://github.com/filebench/filebench).

## Tests description

- Each workload runs for **5 minutes**, using **1 thread** and an IO **block size of 4k**;
- We **compare** the results with the **FUSE passthrough** program;
- In LazyFS, we **vary the page size** (4k and 32k) which generally increases the throughput of read/write operations;

## Workloads

We run sequential and randomized versions of read and write workloads. Also there are macro workloads that mix these operations and increase the work to be done. Finally, some metadata intensive tests were also performed.

### Benchmark status

- **fileserver**:
  - passthrough
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 4k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 32k)
    - [x] run 1
    - [x] run 2
    - [x] run 3

- **varmail**
  - passthrough
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 4k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 32k)
    - [x] run 1
    - [x] run 2
    - [x] run 3

- **webserver**
  - passthrough
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 4k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 32k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
- **filemicro_seqread**
  - passthrough
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 4k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 32k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
- **filemicro_seqwrite**
  - passthrough
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 4k)
      - [x] run 1
      - [x] run 2
      - [x] run 3
  - lazyfs (page = 32k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
- **filemicro_rread**
  - passthrough
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 4k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 32k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
- **filemicro_rwrite**
  - passthrough
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 4k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 32k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
- **filemicro_delete**
  - passthrough
    - [ ] run 1
    - [ ] run 2
    - [ ] run 3
  - lazyfs (page = 4k)
    - [ ] run 1
    - [ ] run 2
    - [ ] run 3
  - lazyfs (page = 32k)
    - [ ] run 1
    - [ ] run 2
    - [ ] run 3
- **filemicro_createfiles**
  - passthrough
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 4k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
  - lazyfs (page = 32k)
    - [x] run 1
    - [x] run 2
    - [x] run 3
