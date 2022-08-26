#!/bin/bash

source  test_common.sh
(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && source configs/conf.sh && source common.sh && export testlog=$logdir/testlog

[ -z $vmip ] &&  echo 'need set $vmip or run in wrapper' && exit 1

export testits=1

function testcase_pre {
	ping_pre
}

function testcase_run {
	runcmd python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -d $vfslot -o 4 -b 1
	sleep 3
	runcmd python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -d $vfslot -o 2
	runcmd python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -d $vfslot -o 3
	runcmd python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -d $vfslot -o 5 -b 1 

}

function testcase_check {
	local vlog=`tail -n 30 $vdpalog`
	grep "last desc pass" <<< "$vlog" || { echo $vlog; return 1; }
	grep "used ring pass" <<< "$vlog" || { echo $vlog; return 1; }
}

function testcase_clean {
	ping_clean
	stop_vdpa_vm
	start_vdpa_vm
}

if [ $sourced -eq 0 ]; then
	test_main
fi
