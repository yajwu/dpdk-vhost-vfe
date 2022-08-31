#!/bin/bash

source  test_common.sh
(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && source configs/conf.sh && . common.sh && . test_common.sh && export vdpalog=logs/vdpa.log && export testlog=logs/testlog

export testits=5

function testcase_pre {
	dd_pre
}

function testcase_run {
	loginfo "kill dpdk-vfe-vdpa"
	stop_vdpa || { logerr ">>" && return 1; }

	start_vdpa
	sleep 5 && loginfo "vdpa process `pgrep dpdk-vfe-vdpa`"
	add_pf_vfs
}

function testcase_check {
	dd_check
}

function testcase_clean {
	dd_clean
}

if [ $sourced -eq 0 ]; then
	test_main
fi
