#!/bin/bash


(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && source configs/conf.sh && source common.sh && export testlog=logs/testlog

source  test_common.sh

[ -z $vmip ] &&  echo 'need set $vmip or run in wrapper' && exit 1

export testits=1

function testcase_pre {
	dd_pre
}

function testcase_run {
	loginfo "no op"
}

function testcase_check {
	loginfo "call dd_check"
	dd_check
}

function testcase_clean {
	dd_clean
}

if [ $sourced -eq 0 ]; then
	test_main
fi
