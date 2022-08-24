#!/bin/bash


(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && . configs/conf.sh && . common.sh && export testlog=logs/testlog

.  test_common.sh

[ -z $vmip ] &&  echo 'need set $vmip or run in wrapper' && exit 1

export testits=1

function testcase_pre {
	start_vdpa_vm
	ping_pre
}

function testcase_run {
	echo "No op in run"
	sleep 1
}

function testcase_check {
	ping_check
}

function testcase_clean {
	ping_clean
	stop_vdpa_vm
}

if [ $sourced -eq 0 ]; then
	test_main
fi
