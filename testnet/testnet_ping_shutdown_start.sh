#!/bin/bash

source  test_common.sh
(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && source configs/conf.sh && . common.sh && . test_common.sh && export vdpalog=logs/vdpa.log && export testlog=logs/testlog

export testits=3

function testcase_pre {
	start_vdpa_vm
	ping_pre
}

function testcase_run {
	ping_check || return 1

	loginfo "shutdown vm "
	runcmd virsh shutdown $vmname
	runcmd sleep 10
	vm_check_down $hname $vmname || return 1

	start_vm
	ping_pre
	ping_check || return 1
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
