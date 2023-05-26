#!/bin/bash

set -x


./gen_table.sh

sshpass -p 3tango ssh gen-l-vrt-439 "cd /images/testvfe/web; /images/testvfe/web/gen_table.sh"
sshpass -p 3tango scp gen-l-vrt-439:/images/testvfe/web/gen-l-vrt-439.csv .

cat gen-l-vrt-440.csv gen-l-vrt-439.csv > a.csv

./gen_html.py

cp index.html /var/www/html
