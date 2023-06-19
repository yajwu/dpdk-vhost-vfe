#!/bin/bash

mon=${1}

results=`ls -t /images/nightly/${mon}_*/result`
fname=`hostname`-$mon.csv

function getver() {
    local fname=$1
    local fw=`grep FW $1 | head -n 1`
    local bfb=`grep bfb $1 | head -n 1`
    local snap=`grep snap $1 | head -n 1`
    local controller=`grep controller $1 | head -n 1`
    local nvmx=`grep nvmx $1 | head -n 1`
    local dpdk=`grep dpdk $1 | head -n 1`
    local qemu=`grep qemu $1 | head -n 1`

    echo "$fw@$bfb@$snap@$controller@$nvmx@$dpdk@$qemu"
}

> $fname
for i in $results; do
        dir=`dirname $i`
        version=`getver $dir/version`
        date=`basename $dir`
        content=`cat $i |cut -d ' ' -f 1,3 | tr '\n' '@'`
        printf "%s,%s,%s,%s,\n"  "$date" `hostname` "${version} " "$content" >> $fname
done
