#!/bin/bash

echo "Building power library test..."

# Compile modbus library (C)
echo "modbus..."
gcc -c modbus.c -o modbus.o

# Compile power library test (C++) and modbus library (C)
echo "power-test..."
g++ -o power-test power_library_test.cpp modbus.o -std=c++17 -lgpiod

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful, ./power-test available. Test run:"
    ./power-test
else
    echo "Build failed."
    exit 1
fi
