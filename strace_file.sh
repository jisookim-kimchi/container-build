#!/bin/bash

strace -f -e trace=%file -o ./logs/strace_file.log "$@"