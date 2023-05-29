#!/bin/bash

mon=${1}

results=`ls -t /images/nightly/${mon}_*/result`
fname=`hostname`-$mon.csv

> $fname
for i in $results; do
        dir=`dirname $i`
	version=`cat $dir/version | grep FW | head -n 1`
        date=`basename $dir`
        content=`cat $i |cut -d ' ' -f 1,3 | tr '\n' '@'`
        printf "%s,%s,%s,%s,\n"  "$date" `hostname` "${version} " "$content" >> $fname
done
