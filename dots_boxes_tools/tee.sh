#!/bin/bash
tee $(dirname "$0")/in_$$.log | "$@"
