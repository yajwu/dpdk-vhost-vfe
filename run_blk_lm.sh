#!/bin/bash

[ "$EUID" -ne 0 ] && echo "Please run as root" && exit 1

export cases="testcas_dd_lm.sh"
#export cases="testcase_ping_restart_vdpa.sh"
./run_blk.sh
