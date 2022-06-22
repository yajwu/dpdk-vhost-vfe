#!/bin/bash


(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && source configs/conf.sh && source common.sh && export testlog=logs/testlog

source  test_common.sh

[ -z $vmip ] &&  echo 'need set $vmip or run in wrapper' && exit 1

export testits=1

function testcase_pre {
	wait_vm
}

function testcase_run {
	vm_check_running $hname $vmname || return 1
	runsshcmd $vmip 'dd if=/dev/vda of=/dev/null bs=4k count=9999'
}

function testcase_check {
:
}

function testcase_clean {
:
}

if [ $sourced -eq 0 ]; then
	test_main
fi
