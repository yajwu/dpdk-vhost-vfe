#!/bin/bash


(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && . configs/conf.sh && . common.sh && export testlog=logs/testlog

.  test_common.sh

[ -z $vmip ] &&  echo 'need set $vmip or run in wrapper' && exit 1

export testits=$vmqps

function testcase_pre {
	ping_pre_without_start
}

function testcase_run {
	#echo "No op in run"
        runsshcmd $vmip ethtool -L $vmeth combined $it
	runsshcmd_bg "$vmip ping $testbed_br_ip -i 0.6 |tee -a $testlog"
	runcmd_bg "ping $vmethip -i 0.6 |tee -a $testlog"
	export ping_pid=$!

	sleep 1
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
