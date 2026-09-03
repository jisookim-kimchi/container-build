#!/bin/bash

strace -f -e trace=%process -o ./logs/strace_process.log "$@"