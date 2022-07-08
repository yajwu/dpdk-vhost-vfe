#!/bin/bash

(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && source configs/conf.sh && . common.sh && . test_common.sh && export testlog=$logdir/testlog

[ -z $vmip ] &&  echo 'need set $vmip or run in wrapper' && exit 1

export testits=5

function testcase_pre {
	ping_pre
}

function testcase_run {
	virsh suspend $vmname || return 1
	virsh resume $vmname || return 1
	sleep 2
}

function testcase_check {
	ping_check
}

function testcase_clean {
	pkill -SIGTERM -f ping
}

if [ $sourced -eq 0 ]; then
	test_main
fi
