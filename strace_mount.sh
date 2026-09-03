#!/bin/bash

strace -f -e trace=mount,umount,umount2,pivot_root -o ./logs/strace_mount.log ./container