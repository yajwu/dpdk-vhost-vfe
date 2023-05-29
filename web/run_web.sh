#!/bin/bash

set -x

mon=`date +"%Y_%m"`

cd /images/testvfe/web

./gen_table.sh $mon

sshpass -p 3tango ssh gen-l-vrt-439 "cd /images/testvfe/web; /images/testvfe/web/gen_table.sh $mon"
sshpass -p 3tango scp gen-l-vrt-439:/images/testvfe/web/gen-l-vrt-439-$mon.csv .

cat gen-l-vrt-440-$mon.csv gen-l-vrt-439-$mon.csv > a.csv
sort a.csv -r -o a.csv

./gen_html.py $mon

cp index.html $mon.html /var/www/html
