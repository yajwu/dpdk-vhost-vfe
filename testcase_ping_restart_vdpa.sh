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
	pkill dpdk-vdpa || pkill -9 dpak-vdpa
	pgrep dpdk-vdpa && sleep 2 && pgrep dpdk-vdpa && sleep 4
	pgrep dpdk-vdpa && logerr "kill vdpa fail" && return 1

	./vdpacmd >> $vdpalog 2>&1 &
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
