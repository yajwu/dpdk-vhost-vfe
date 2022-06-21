#!/bin/bash

[ "$EUID" -ne 0 ] && echo "Please run as root" && exit 1

export cases="testcase_ping.sh"
./run.sh

