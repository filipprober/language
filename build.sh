#!/bin/bash

echo "Building mylang with LLVM..."

LLVM_CONFIG="/opt/homebrew/opt/llvm/bin/llvm-config"

if ! command -v $LLVM_CONFIG &> /dev/null; then
    echo "Error: llvm-config not found!"
    exit 1
fi

echo "Using LLVM version: $($LLVM_CONFIG --version)"

# System clang mit allen paths
/usr/bin/clang++ -std=c++20 -g \
    -I$($LLVM_CONFIG --includedir) \
    -L$($LLVM_CONFIG --libdir) \
    -L/opt/homebrew/lib \
    -fexceptions -frtti \
    -Wno-deprecated-declarations \
    -o mylang main.cpp \
    $($LLVM_CONFIG --libs core support irreader aarch64asmparser aarch64codegen aarch64desc aarch64disassembler aarch64info aarch64utils) \
    $($LLVM_CONFIG --system-libs) \
    -lz -lzstd

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Run with: ./mylang <file.ml>"
else
    echo "✗ Build failed!"
    exit 1
fi