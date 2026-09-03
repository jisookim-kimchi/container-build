#!/bin/bash
strace -f -o ./logs/strace_error.log "$@"

grep "= -1" ./logs/strace_error.log