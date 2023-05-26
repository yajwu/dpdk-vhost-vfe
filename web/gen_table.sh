#!/bin/bash

mon="2023_05_*"

results=`ls -t /images/nightly/$mon/result`
fname=`hostname`.csv

> $fname
for i in $results; do
        dir=`dirname $i`
	version=`cat $dir/version | grep FW | head -n 1`
        date=`basename $dir`
        content=`cat $i |cut -d ' ' -f 1,3 | tr '\n' '@'`
        printf "%s,%s,%s,%s,\n"  "$date" `hostname` "${version} " "$content" >> $fname
done
