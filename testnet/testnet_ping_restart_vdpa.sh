#!/bin/bash

source  test_common.sh
(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && source configs/conf.sh && . common.sh && . test_common.sh && export vdpalog=logs/vdpa.log && export testlog=logs/testlog

export testits=5

function testcase_pre {
	ping_pre
}

function testcase_run {
	loginfo "kill dpdk-vdpa"
	stop_vdpa || { logerr ">>" && return 1; }

	start_vdpa
	sleep 5 && loginfo "vdpa process `pgrep dpdk-vdpa`"
}

function testcase_check {
	ping_check
}

function testcase_clean {
	ping_clean
}

if [ $sourced -eq 0 ]; then
	test_main
fi
