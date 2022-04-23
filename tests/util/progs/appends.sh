#!/bin/bash

FILE=/tmp/lazyfs/file
APPEND_EVERY=0.1

for i in {1..100}
do
    echo $i >> $FILE
    sleep $APPEND_EVERY
    if ! (( $i % 10 )); then
        echo "clearing cache..."
        echo "lazyfs::clear-cache" > ~/faults-example.fifo
    fi
done

echo "applying 'cat' to file..."
cat $FILE
echo "done"
