#!/usr/bin/python3

#####################################################################################
# lfscheck : Checks the consistency of LazyFS' after clear cache and sync operations                                                                                              
# Author   : João Azevedo
# Email    : joao.azevedo@inesctec.pt
#
# Copyright (c) 2020-2022 INESC TEC.
#####################################################################################

import hashlib
import logging
import os
import random
import time
import sys
import argparse
import traceback

from datetime import timedelta

# Default filename for tests
# External scripts can use this to deploy multi-process tests
default_test_filename = "file_{}".format(time.strftime("%Y%m%d-%H%M%S"))

# A simple Argparser for lfscheck

parser = argparse.ArgumentParser(prog="lfscheck", description="Validate LazyFS consistency after cache clearing and fsyncing.", epilog="Generates random operations of write/fsync/clear-cache to validate the consistency after syncing data and injecting faults.")
parser.add_argument("-m", "--lazyfs.mount", metavar="mount_point", help="The full path of the LazyFS's mount point.", required=True)
parser.add_argument("-f", "--lazyfs.fifo", metavar="fifo", help="The full path of the LazyFS's faults fifo path.", required=True)
parser.add_argument("-t", "--test.time", type=int, metavar="time", help="Test total execution time.", required=True)
parser.add_argument("-i", "--test.file", type=str, metavar="file", help="File to execute operations.", required=False, default=default_test_filename)
parser.add_argument("-min", "--test.min.offset", type=int, metavar="min off", help="Minimum offset to perform writes.", required=False, default=0)
parser.add_argument("-max", "--test.max.offset", type=int, metavar="max off", help="Maximum offset to perform writes.", required=False, default=12288)

args = parser.parse_args()

# Logging configuration

logger    = logging.getLogger(getattr(args, 'test.file').replace('\\',''))
ch        = logging.StreamHandler()
formatter = logging.Formatter("%(levelname)s %(asctime)s - %(name)s - %(message)s")
ch.setFormatter(formatter)
logger.addHandler(ch)
logger.setLevel(logging.INFO)

# LazyFS's mount point
LAZYFS_MOUNT       = getattr(args, 'lazyfs.mount')
# LazyFS's FIFO path
LAZYFS_FAULTS_FIFO = getattr(args, 'lazyfs.fifo')
# LazyFS's clear cache command syntax
LAZYFS_CLEAR_CACHE_COMMAND="lazyfs::clear-cache\n"

# Check whether the specified files exist

if not os.path.isdir(LAZYFS_MOUNT):
    logger.error("The mount point '{}' does not exist.".format(LAZYFS_MOUNT))
    sys.exit(-1)

if not os.path.exists(LAZYFS_FAULTS_FIFO):
    logger.error("The fifo file '{}' does not exist.".format(LAZYFS_FAULTS_FIFO))
    sys.exit(-1)

# Test workload options

# Total time the test will run
TEST_RUNTIME  = getattr(args, 'test.time')
# Test end time
TEST_END = time.time() + TEST_RUNTIME
# The file to perform operations
TEST_FILENAME = "{}/{}".format(LAZYFS_MOUNT, getattr(args, 'test.file').replace('\\',''))
# Operation types to handle data consistency
OPERATIONS=["sync", "clear_cache"]
# Min offset to perform writes
WRITE_MIN_OFFSET=getattr(args, 'test.min.offset')
# Max offset to perform writes
WRITE_MAX_OFFSET=getattr(args, 'test.max.offset')
# Delay added after each consistency operation (clear or sync)
TIME_SLEEP_AFTER_OPERATION=0.1
# A buffer to track file contents
FILE_TRACK_BUFFER=bytearray(WRITE_MAX_OFFSET)
# The tracked file size
FILE_TRACK_SIZE=0

# Checks if two buffers are equal via md5 hashing of bytearrays
def check_bytes_equality(bytearr1, bytearr2):
    return hashlib.md5(bytearr1).digest() == hashlib.md5(bytearr2).digest()

logger.info("starting consistency checking test")

fd = os.open(TEST_FILENAME, os.O_CREAT|os.O_RDWR|os.O_TRUNC)
logger.info("create: filename={}".format(TEST_FILENAME))

if fd < 0:
    logger.error("create: filename={} FAILED".format(TEST_FILENAME))
    sys.exit(-1)

logger.info("")
start_execution = time.time()
errors          = False

try:

    while time.time() < TEST_END:
        
        # save last history

        LAST_FILE_SIZE = FILE_TRACK_SIZE

        # write request

        write_from = random.randint(WRITE_MIN_OFFSET, WRITE_MAX_OFFSET)
        write_to   = random.randint(write_from, WRITE_MAX_OFFSET)
        write_size = write_to - write_from

        if write_size == 0:
            continue

        write_buf  = bytearray([random.randint(0, 10)] * write_size)
        
        logger.info("pwrite: w_from={},w_to={},w_size={}".format(write_from, write_to, write_size))

        written = os.pwrite(fd, write_buf, write_from)

        # update file size if write > curr offset size

        FILE_TRACK_SIZE = max(FILE_TRACK_SIZE, write_to)

        logger.info("result: wrote={},expected={}".format(written, write_size))
        assert written == write_size
        
        # check file contents

        read_result = os.pread(fd, write_size, write_from)
        assert check_bytes_equality(read_result, write_buf) == True
        
        # random choice: clear_cache | fsync

        operation = random.choice(OPERATIONS)

        logger.info("consistency: operation={}".format(operation))
        if operation == "clear_cache":
        
            # send lazyfs::clear-cache > fifo path
            
            faults_pipe_fd = os.open(LAZYFS_FAULTS_FIFO, os.O_WRONLY)
            os.write(faults_pipe_fd, bytes(LAZYFS_CLEAR_CACHE_COMMAND, 'ascii'))
            os.close(faults_pipe_fd)
            
            # update file size, if needed
            
            FILE_TRACK_SIZE=LAST_FILE_SIZE
            logger.info("clear_cache_request: sent!")
        
        elif operation == "sync":
            
            # fsync file

            os.fsync(fd)
            
            faults_pipe_fd = os.open(LAZYFS_FAULTS_FIFO, os.O_WRONLY)
            os.write(faults_pipe_fd, bytes(LAZYFS_CLEAR_CACHE_COMMAND, 'ascii'))
            os.close(faults_pipe_fd)
            
            # update in-memory array

            for idx in range(len(write_buf)):
                FILE_TRACK_BUFFER[write_from+idx] = write_buf[idx]
            
            logger.info("fsync_request: sent! (and cache cleared after)")
    
        logger.info("sleeping: {} seconds".format(TIME_SLEEP_AFTER_OPERATION))
        time.sleep(TIME_SLEEP_AFTER_OPERATION)

        # check file contents

        read_result = os.pread(fd, WRITE_MAX_OFFSET, 0)
        assert check_bytes_equality(read_result, FILE_TRACK_BUFFER[:FILE_TRACK_SIZE]) == True

        # check file size

        file_nr_bytes = os.lseek(fd, 0, os.SEEK_END)
        logger.info("lseek_request: file_size={},expected={}".format(file_nr_bytes, FILE_TRACK_SIZE))
        assert file_nr_bytes == FILE_TRACK_SIZE
            
        logger.info("")

except AssertionError:
    logger.error("errors during checking!", exc_info=True)
    
logger.info("")

elapsed_time = (time.time() - start_execution)

logger.info("total test time: {}".format(str(timedelta(seconds=elapsed_time))))

# Remove test file

logger.info("remove: filename={}".format(TEST_FILENAME))
os.unlink(TEST_FILENAME)

if not errors:
    logger.info("test finished, everything seems OK")
    sys.exit(0)

sys.exit(-1)
