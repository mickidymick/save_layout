#!/bin/bash
gcc -o save_layout.so save_layout.c $(yed --print-cflags) $(yed --print-ldflags)
