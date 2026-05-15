#!/bin/bash

echo "Building read-voltage..."

# Compile
gcc -o mode set_modbus_read_mode.c modbus.c -lgpiod

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful, ./mode available"
else
    echo "Build failed."
    exit 1
fi
