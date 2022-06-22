#!/bin/bash

(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && . configs/conf.sh && . common.sh && . /mswg/projects/fw/fw_ver/hca_fw_tools/.fwvalias

function cleanup_env {
	runcmd systemctl restart libvirtd
	runcmd virsh shutdown $vmname && sleep 4
	#ping $vmip -c 1 && runsshcmd $vmip shutdown -h now

	pkill -x ping
	pkill -x sshpass

	pkill dpdk-vdpa && sleep 3
	pgrep dpdk-vdpa && ( logerr "kill dpdk-vdpa fail" && exit 1)

	for i in `virsh list --name`; do
		runcmd virsh destroy $i
		sleep 2
	done
}

function init_cleanup_env {
	loginfo init cleanup_env
	cleanup_env
}

function post_cleanup_env {
	loginfo post cleanup_env
	cleanup_env
}

function info_env {
	loginfo "==== info ===="
	flq | tee -a $logdir/info
	systemctl status libvirtd | tee -a $logdir/info
	$qemuapp --version | tee -a $logdir/info
}

function prep_vf_ovs {
	loginfo prep_vf_ovs

	. ./_create_vf_ovs.sh
}

function prep_sw {
	echo "TODO:build/update sw"
}

function start_vdpa {
	loginfo start vdpa

	export vdpalog=$logdir/vdpa.log
	. ./vdpacmd
	sleep 2 && loginfo "vdpa process `pgrep dpdk-vdpa`"
	pgrep dpdk-vdpa || { logerr "bootup dpdk-vdpa fail" ; exit 1; }

	## add vf on bf2
	[[ ${testtype} == "blk" ]] && runbf2cmd $bf2ip 'snap_rpc.py controller_virtio_blk_create mlx5_0 --pf_id 0 --vf_id 0 --bdev_type spdk --bdev Malloc0'
}

function start_vm {
	loginfo start vm

	runcmd virsh create configs/${vmxml}.${testtype}
	sleep 2
	virsh list --all

	local vms=`virsh list --state-running --name`
	[[ "$vms" == *"$2"* ]] || { logerr "vm_check fail: $1 $2" ; exit 1; }
}

function start_vdpa_vm {
	start_vdpa
	start_vm
}

function start_peer {
    loginfo start test env on $peer
    runsshcmd_bg $peer '\(cd /images/testvfe \;  ./run.sh slave \; hostname\)'
}

function stop_peer {
     echo "todo check on exit file"
}


if [ $sourced -eq 0 ]; then
	#info_env
	#prep_vf_ovs
	init_cleanup_env
	start_vdpa_vm
fi

