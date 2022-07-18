#!/bin/bash

(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && . configs/conf.sh && . common.sh && . /mswg/projects/fw/fw_ver/hca_fw_tools/.fwvalias

function restart_mlnx_snap {
	if [[ $testtype == "blk" ]]; then
		# work around to restart snap
		loginfo "restart mlnx_snap on bf2"
		runbf2cmd $bf2ip 'systemctl restart mlnx_snap'
		runbf2cmd $bf2ip 'spdk_rpc.py bdev_null_create Null0 1024 512'
		runbf2cmd $bf2ip 'snap_rpc.py controller_virtio_blk_create --pf_id 0 --bdev_type spdk mlx5_0 --bdev Null0 --num_queues 1  --admin_q'
	fi
}

function stop_vdpa {
	pkill dpdk-vfe-vdpa && sleep 3 && pgrep dpdk-vfe-vdpa && sleep 5
	pgrep dpdk-vfe-vdpa && { logerr "kill dpdk-vfe-vdpa fail" && return 1; }


	return 0
}

function stop_vm {
	virsh list | grep -q $vmname && virsh destroy $vmname
}

function cleanup_env {
	pkill -x ping
	pkill -x sshpass

	stop_vdpa
	stop_vm

}

function init_cleanup_env {
	loginfo init cleanup_env
	cleanup_env
	runcmd systemctl restart libvirtd
	restart_mlnx_snap
}

function post_cleanup_env {
	loginfo post cleanup_env
	cleanup_env
}

function info_env {
	loginfo "==== info ===="
	flq | tee -a $logdir/info
	#systemctl status libvirtd | tee -a $logdir/info
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

	#restart_mlnx_snap

	export vdpalog=$logdir/vdpa.log
	. ./vdpacmd
	sleep 2 && loginfo "vdpa process `pgrep dpdk-vfe-vdpa`"
	pgrep dpdk-vfe-vdpa || { logerr "bootup dpdk-vfe-vdpa fail" ; exit 1; }


	runcmd python sw/dpdk/app/vfe-vdpa/vhostmgmt mgmtpf -a ${pfslot}
	## add vf on bf2
	[[ ${testtype} == "blk" ]] && runbf2cmd $bf2ip 'snap_rpc.py controller_virtio_blk_create mlx5_0 --pf_id 0 --vf_id 0 --bdev_type spdk --bdev Null0'
	sleep 1
	runcmd python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a ${vfslot}

	[[ ${testtype} == "net" ]] && runbf2cmd $bf2ip virtnet modify -p 0 -v 0 device -m $devmac

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
	runsshcmd_bg $peer "\(cd /images/testvfe \;  ./run_${1}.sh slave \; hostname\)"
}

function stop_peer {
     echo "todo check on exit file"
}


if [ $sourced -eq 0 ]; then
	#info_env
	#prep_vf_ovs
	netfn=`readlink  /sys/bus/pci/devices/${netpf}/virtfn0`
	blkfn=`readlink  /sys/bus/pci/devices/${blkpf}/virtfn0`
	export netvf0=`basename $netfn`
	export blkvf0=`basename $blkfn`

	export testtype=blk

	export pfslot=$blkpf
	export vfslot=$blkvf0

	loginfo $netvf0 pfslot: $pfslot vfslot: $vfslot
	init_cleanup_env
	start_vdpa_vm
fi

