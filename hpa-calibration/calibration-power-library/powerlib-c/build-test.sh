#!/bin/bash

echo "Building power library test..."

# Compile everything
gcc -o power-test power_library_test.c power_library.c modbus.c -lgpiod

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful, ./power-test available. Test run:"
    ./power-test
else
    echo "Build failed."
    exit 1
fi
