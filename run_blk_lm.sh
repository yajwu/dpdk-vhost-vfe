#!/bin/bash

[ "$EUID" -ne 0 ] && echo "Please run as root" && exit 1

export cases="testblk/testblk_dd_lm.sh"
#export cases="testblk/testblk_ping_restart_vdpa.sh"
./run_blk.sh
