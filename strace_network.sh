#!/bin/bash

strace -f -e trace=%network -o ./logs/strace_network.log "$@"