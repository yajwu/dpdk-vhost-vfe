#!/bin/bash

now=`date +"%Y_%m_%d_%H_%M"`
basedir=/images/nightly

ldir=$basedir/$now
mkdir -p $ldir

ln -sfn  $ldir latest


cd /images/testvfe

tools/update_bundle.sh | tee $ldir/update_bundle.log
./tools/_show_ver.sh | tee $ldir/version
dmesg -c

./run_net.sh
cp -rf latest $ldir/run_net_log
dmseg -c | tee $ldir/run_net_log/dmesg


./run_blk.sh
cp -rf latest $ldir/run_blk_log
dmesg -c | tee $ldir/run_blk_log/dmesg

#./run_mul.sh
#cp -rf latest $ldir/run_mul_log

cat $ldir/run_net_log/result $ldir/run_blk_log/result $ldir/run_mul_log/result > $ldir/result
