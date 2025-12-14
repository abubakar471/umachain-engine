#!/bin/bash
set -e

echo "Building C++ server..."
make

echo "Starting server..."
./server
