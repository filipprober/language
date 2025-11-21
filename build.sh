#!/bin/bash

echo "Building mylang with LLVM..."

ARCHITECTURE=$(uname -m)

echo "Detected architecture: $ARCHITECTURE"

if [ "$ARCHITECTURE" = "arm64" ] || [ "$ARCHITECTURE" = "aarch64" ]; then
    LLVM_CONFIG="/opt/homebrew/opt/llvm/bin/llvm-config"
    HOMEBREW_LIB="/opt/homebrew/lib"
    echo "Using ARM64 LLVM (Homebrew)"
elif [ "$ARCHITECTURE" = "x86_64" ]; then
    LLVM_CONFIG="/usr/local/opt/llvm/bin/llvm-config"
    HOMEBREW_LIB="/usr/local/lib"
    echo "Using x86_64 LLVM"
else
    echo "Error: Unsupported architecture: $ARCHITECTURE"
    exit 1
fi

if ! command -v $LLVM_CONFIG &> /dev/null; then
    echo "Error: llvm-config not found at $LLVM_CONFIG!"
    echo "Please install LLVM via Homebrew: brew install llvm"
    exit 1
fi
echo "Using LLVM version: $($LLVM_CONFIG --version)"

/usr/bin/clang++ -std=c++20 -g \
    -I$($LLVM_CONFIG --includedir) \
    -L$($LLVM_CONFIG --libdir) \
    -L$HOMEBREW_LIB \
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