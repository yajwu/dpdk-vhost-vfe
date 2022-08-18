#!/bin/bash

[ "$EUID" -ne 0 ] && echo "Please run as root" && exit 1

#export cases="testnet/testnet_ping_00.sh"
#export cases="testnet/testnet_ping_restart_vdpa.sh"
#export cases="testnet/testnet_ping_suspend_resume.sh"
#export cases="testnet/testnet_ping_dirtybit.sh"
#export cases="testnet/testnet_ping_shutdown_start.sh"
#export cases="testnet/testnet_ping_qpnum_chng.sh"
export cases="testcas_ping_lm.sh"

./run_net.sh

