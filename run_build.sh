#!/bin/bash

# Define the project directory
PROJECT_DIR="/home/ubuntu/deribit_trading_system"
BUILD_DIR="$PROJECT_DIR/build"

# Navigate to the project directory
cd "$PROJECT_DIR" || { echo "Failed to navigate to project directory"; exit 1; }

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    mkdir "$BUILD_DIR"
fi

# Navigate to build directory
cd "$BUILD_DIR" || { echo "Failed to navigate to build directory"; exit 1; }

# Run CMake and Make
cmake .. && make || { echo "Build failed"; exit 1; }

# Navigate out of the build directory
cd "$PROJECT_DIR" || exit

# Run the compiled executable
./build/main
