#!/bin/sh

set -eux

gcc main.c -Iinclude -o main -lm
./main