#!/bin/bash

echo "Building read-voltage..."

# Compile
gcc -o read-voltage read_voltage.c modbus.c -lgpiod

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful, ./read-voltage available. Test run:"
    ./read-voltage
else
    echo "Build failed."
    exit 1
fi
