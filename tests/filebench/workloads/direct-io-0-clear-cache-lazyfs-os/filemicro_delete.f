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

# Create a fileset of 50,000 entries ($nfiles), where each file's size is set
# via a gamma distribution with the median size of 16KB ($filesize).
# Fire off 16 threads ($nthreads), where each thread stops after
# deleting 1000 ($count) files.

set $WORKLOAD_PATH="/mnt/test-fs/lazyfs/fb-workload"

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=500000, dirwidth=1000,
               dirgamma=0, filesize=4k

define process name="process-1", instances=1
{
    thread name="thread-1", memsize=4k, instances=1
    {
        flowop deletefile name="deletefile-1", filesetname="fileset-1", iters=500000

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

psrun -5 300