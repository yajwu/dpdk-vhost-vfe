#!/bin/bash


source  test_common.sh
(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && source configs/conf.sh && source common.sh && export testlog=$logdir/testlog

[ -z $vmip ] &&  echo 'need set $vmip or run in wrapper' && exit 1

export testits=5

function testcase_pre {
	runsshcmd $peer virsh destroy $vmname
	start_peer net
	ping_pre
	# with VIRTIO_NET_F_GUEST_ANNOUNCE, no need bi-direction ping
	#runsshcmd_bg $vmip ping $p0ip -i 0.5

	runcmd virsh list --all
	runsshcmd $peer virsh list --all
	vm_check_running $hname $vmname || return 1
	vm_check_down $peer $vmname || return 1

	# todo add check instead of sleep
	sleep 30
}

function testcase_run {

	runcmd_bg virsh migrate --verbose --live --persistent $vmname qemu+ssh://$peer/system  --unsafe
	sleep 10
	vm_check_running $peer $vmname || return 1
	vm_check_down $hname $vmname || return 1
	ping_check || return 1

	runsshcmd_bg $peer virsh migrate --verbose --live --persistent $vmname qemu+ssh://$hname/system  --unsafe
	sleep 10
	vm_check_running $hname $vmname || return 1
	vm_check_down $peer $vmname || return 1
	ping_check || return 1
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
