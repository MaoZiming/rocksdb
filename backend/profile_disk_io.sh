#!/bin/bash

# Check if the user provided a directory to test
if [ -z "$1" ]; then
  echo "Usage: $0 <directory_to_test>"
  exit 1
fi

TARGET_DIR=$1

# Function to measure sequential write speed using 'dd'
function measure_sequential_write() {
  echo "Measuring sequential write speed with dd..."
  sync
  dd if=/dev/zero of=$TARGET_DIR/testfile bs=1M count=1024 conv=fdatasync,notrunc oflag=direct 2>&1 | grep -E 'copied'
  sync
}

# Function to measure sequential read speed using 'dd'
function measure_sequential_read() {
  echo "Measuring sequential read speed with dd..."
  sync
  dd if=$TARGET_DIR/testfile of=/dev/null bs=1M 2>&1 | grep -E 'copied'
  sync
}

# Function to measure random I/O using 'ioping'
function measure_random_io() {
  echo "Measuring random I/O performance with ioping..."
  ioping -c 10 $TARGET_DIR
}

# Function to measure random read/write with fio
function measure_fio() {
  echo "Measuring random read/write with fio..."
  fio --name=rand_rw_test --directory=$TARGET_DIR --rw=randrw --size=1G --numjobs=1 --time_based --runtime=60 --group_reporting --bs=4k --ioengine=libaio --direct=1 --iodepth=16
}

# Cleanup test file
function cleanup() {
  echo "Cleaning up..."
  rm -f $TARGET_DIR/testfile
}

# Run the tests
measure_sequential_write
measure_sequential_read
measure_random_io
measure_fio

# Clean up after tests
cleanup

echo "Disk I/O profiling complete."
