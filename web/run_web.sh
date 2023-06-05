#!/bin/bash

set -x

mon=${1:-`date +"%Y_%m"`}

cd /images/testvfe/web

./gen_table.sh $mon

sshpass -p 3tango ssh gen-l-vrt-439 "cd /images/testvfe/web; /images/testvfe/web/gen_table.sh $mon"
sshpass -p 3tango scp gen-l-vrt-439:/images/testvfe/web/gen-l-vrt-439-$mon.csv .


sshpass -p 3tango ssh gen-l-vrt-439 "cd /images/testbf3/web; /images/testbf3/web/gen_table.sh $mon"
sshpass -p 3tango scp gen-l-vrt-439:/images/testbf3/web/gen-l-vrt-439-bf3-$mon.csv .

>a.csv
cat gen-l-vrt-440-$mon.csv gen-l-vrt-439-$mon.csv gen-l-vrt-439-bf3-$mon.csv > a.csv
sort a.csv -r -o a.csv

./gen_html.py $mon

cp index.html $mon.html /var/www/html
