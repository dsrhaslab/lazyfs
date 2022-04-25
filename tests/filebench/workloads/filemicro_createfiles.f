#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"

# Creates a fileset with 20,000 entries ($nfiles), but only preallocates
# 50% of the files.  Each file's size is set via a gamma distribution with
# a median size of 1KB ($filesize).
#
# The single thread then creates a new file and writes the whole file with
# 1MB I/Os.  The thread stops after 5000 files ($count/num of flowops) have
# been created and written to.

set $WORKLOAD_PATH="/mnt/test-fs/lazyfs/fb-workload"

define flowop name=createwriteclose
{
    flowop createfile name="createfile-1", filesetname="fileset-1", fd=1
    flowop write name="write-1", fd=1, iosize=4k
    flowop closefile name="closefile-1", fd=1
}

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=1000000, dirwidth=1000,
               dirgamma=0, filesize=4k

define process name="process-1", instances=1
{
    thread name="thread-1", memsize=4k, instances=1
    {
        flowop createwriteclose name="createwriteclose-1", iters=1000000

        flowop finishoncount name="finishoncount-1", value=1
    }
}

# explicitly preallocate files
create files

# drop file system caches
system "sync ."
system "echo 3 > /proc/sys/vm/drop_caches"

echo "time sync"
system "date '+time sync %s.%N'"
echo "time sync"

run 5 300
