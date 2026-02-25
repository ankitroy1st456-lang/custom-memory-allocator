# Custom Memory Allocator in C++

## Overview
This project is a custom memory allocator built in C++ to explore how `malloc` and `free` work internally.

## Features
- Fixed-size pools (1 MB each)
- Segregated free lists by block size classes
- 16-byte alignment
- Block splitting and expansion
- Bidirectional coalescing

## Demo
Compile and run:
```bash
g++ code.cpp -o allocator
./allocator
