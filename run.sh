#!/bin/sh

set -eux

gcc main.c -Iinclude -o main -lm -lavformat -lavcodec -lswresample -lswscale -lavutil
./main