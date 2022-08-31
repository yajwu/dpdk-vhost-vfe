#!/bin/bash

(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && . configs/conf.sh && . common.sh && . /mswg/projects/fw/fw_ver/hca_fw_tools/.fwvalias

function restart_mlnx_snap {
	if [[ $testtype == "blk" || $testtype == "mul" ]]; then
		# work around to restart snap
		loginfo "restart mlnx_snap on bf2"
		runbf2cmd $bf2ip 'systemctl restart mlnx_snap'
		runbf2cmd $bf2ip 'spdk_rpc.py bdev_null_create Null0 1024 512'
		runbf2cmd $bf2ip 'snap_rpc.py controller_virtio_blk_create --pf_id 0 --bdev_type spdk mlx5_0 --bdev Null0 --num_queues 1  --admin_q'
	fi
}

function restart_controller {
	if [[ $testtype == "net" || $testtype == "mul" ]]; then
		# work around to restart snap
		loginfo "remove !!!!restart virtio-net-controller on bf2 as W.A."
		runbf2cmd $bf2ip 'systemctl restart virtio-net-controller'
		runcmd sleep 20
		runbf2cmd $bf2ip 'virtnet modify -p 0 device -f 0x22300470028'
	fi
}

function stop_vdpa {
	loginfo "stop_vdpa begin" `pgrep dpdk-vfe-vdpa`

	while pgrep dpdk-vfe-vdpa ; do
		pkill -SIGTERM dpdk-vfe-vdpa && runcmd sleep 2 
	done
	loginfo "stop_vdpa end" `pgrep dpdk-vfe-vdpa`

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
	restart_controller
}

function post_cleanup_env {
	loginfo post cleanup_env
	cleanup_env

	runbf2cmd $bf2ip 'journalctl -u virtio-net-controller  -n 100000 > $logdir/virtio-net-controller.log'
	#pkill -x tee
	pkill -x tail
}

function info_env {
	loginfo "==== info ===="
}

function prep_vf_ovs {
	loginfo prep_vf_ovs

	. ./_create_vf_ovs.sh
}

function prep_sw {
	echo "TODO:build/update sw"
}

function add_pf_vfs() {
	#runcmd sleep 2
	runcmd python sw/dpdk/app/vfe-vdpa/vhostmgmt mgmtpf -a ${pfslot}
	runcmd sleep 2

	## add vf on bf2
	[[ ${testtype} == "blk" ]] && runbf2cmd $bf2ip 'snap_rpc.py controller_virtio_blk_create mlx5_0 --pf_id 0 --vf_id 0 --bdev_type spdk --bdev Null0'
	sleep 4
	[[ ${testtype} == "net" ]] && runbf2cmd $bf2ip virtnet modify -p 0 -v 0 device -m $devmac
	sleep 1
	runcmd python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a ${vfslot}
}

function start_vdpa {
	loginfo start_vdpa `pgrep dpdk-vfe-vdpa`

	#restart_mlnx_snap
	#restart_controller

	export vdpalog=$logdir/vdpa.log
	. ./vdpacmd
	sleep 2 && loginfo "vdpa process: `pgrep dpdk-vfe-vdpa`"

	pgrep dpdk-vfe-vdpa || { logerr "bootup dpdk-vfe-vdpa fail" ; return 1; }


}

function start_vm {
	loginfo start_vm

	runcmd virsh create configs/${vmxml}.${testtype}
	sleep 2
	virsh list --all

	local vms=`virsh list --state-running --name`
	[[ "$vms" == *"$2"* ]] || { logerr "vm_check fail: $1 $2" ; exit 1; }
}

function start_vdpa_vm {
	start_vdpa
	add_pf_vfs
	start_vm
}

function stop_vdpa_vm {
	stop_vdpa
	stop_vm
}

function start_peer {
    loginfo start test env on $peer
	runsshcmd_bg $peer "\(cd /images/testvfe \;  ./run_${1}.sh slave \; hostname\)"
}

function stop_peer {
     echo "todo check on exit file"
}

# temp put here

function ping_dd_multi_check() {

	for i in {2..16}; do
		ping 1.1.${i}.2 -I 1.1.${i}.1 -c2
		[[ $? -eq 0 ]] || { logerr "ping fail"; return 1; }
	done

	for i in {162..165}; do
		sshpass -p 3tango ssh root@gen-l-vrt-440-${i} dd if=/dev/vda of=/dev/null bs=4k count=999
		[[ $? -eq 0 ]] || { logerr "ping fail"; return 1; }
		sshpass -p 3tango ssh root@gen-l-vrt-440-${i} dd if=/dev/vdb of=/dev/null bs=4k count=999
		[[ $? -eq 0 ]] || { logerr "ping fail"; return 1; }
		sshpass -p 3tango ssh root@gen-l-vrt-440-${i} dd if=/dev/vdc of=/dev/null bs=4k count=999
		[[ $? -eq 0 ]] || { logerr "ping fail"; return 1; }
		sshpass -p 3tango ssh root@gen-l-vrt-440-${i} dd if=/dev/vdd of=/dev/null bs=4k count=999
		[[ $? -eq 0 ]] || { logerr "ping fail"; return 1; }
	done
}

function mul_add_pfs() {
	loginfo "mul_add_pfs"

	python sw/dpdk/app/vfe-vdpa/vhostmgmt mgmtpf -a 0000:3b:00.2
	python sw/dpdk/app/vfe-vdpa/vhostmgmt mgmtpf -a 0000:3b:00.3
	runcmd sleep 3

	sshpass -p centos ssh root@gen-l-vrt-317-bf  'for i in {00..15}; do  virtnet modify -p 0 -v $i device -m 00:00:04:40:62:${i}; done'
	sshpass -p centos ssh root@gen-l-vrt-317-bf  'for i in {0..15}; do snap_rpc.py controller_virtio_blk_create mlx5_0 --pf_id 0 --vf_id $i --bdev_type spdk --bdev Null0; done'

}

function mul_add_vfs() {
	loginfo "add_vfs"

	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:04.5 -v /tmp/vfe-net0
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.5 -v /tmp/vfe-blk0
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:04.6 -v /tmp/vfe-net1
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.6 -v /tmp/vfe-blk1
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:04.7 -v /tmp/vfe-net2
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.7 -v /tmp/vfe-blk2
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:05.0 -v /tmp/vfe-net3
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:07.0 -v /tmp/vfe-blk3
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:05.1 -v /tmp/vfe-net4
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:07.1 -v /tmp/vfe-blk4
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:05.2 -v /tmp/vfe-net5
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:07.2 -v /tmp/vfe-blk5
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:05.3 -v /tmp/vfe-net6
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:07.3 -v /tmp/vfe-blk6
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:05.4 -v /tmp/vfe-net7
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:07.4 -v /tmp/vfe-blk7
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:05.5 -v /tmp/vfe-net8
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:07.5 -v /tmp/vfe-blk8
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:05.6 -v /tmp/vfe-net9
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:07.6 -v /tmp/vfe-blk9
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:05.7 -v /tmp/vfe-net10
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:07.7 -v /tmp/vfe-blk10
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.0 -v /tmp/vfe-net11
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:08.0 -v /tmp/vfe-blk11
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.1 -v /tmp/vfe-net12
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:08.1 -v /tmp/vfe-blk12
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.2 -v /tmp/vfe-net13
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:08.2 -v /tmp/vfe-blk13
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.3 -v /tmp/vfe-net14
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:08.3 -v /tmp/vfe-blk14
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.4 -v /tmp/vfe-net15
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:08.4 -v /tmp/vfe-blk15

	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf 0000:3b:00.2 -l | egrep 'net|blk'
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf 0000:3b:00.3 -l | egrep 'net|blk'

}

function mul_del_vfs() {
	loginfo "del_vfs"

	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:04.5
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:06.5
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:04.6
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:06.6
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:04.7
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:06.7
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:05.0
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:07.0
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:05.1
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:07.1
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:05.2
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:07.2
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:05.3
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:07.3
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:05.4
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:07.4
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:05.5
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:07.5
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:05.6
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:07.6
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:05.7
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:07.7
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:06.0
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:08.0
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:06.1
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:08.1
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:06.2
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:08.2
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:06.3
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:08.3
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:06.4
	python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -r 0000:3b:08.4

}


function mul_create_vms() {
	loginfo "create_vms"

	virsh create configs/4_4/gen-l-vrt-440-162-CentOS-7.4.xml
	virsh create configs/4_4/gen-l-vrt-440-163-CentOS-7.4.xml
	virsh create configs/4_4/gen-l-vrt-440-164-CentOS-7.4.xml
	virsh create configs/4_4/gen-l-vrt-440-165-CentOS-7.4.xml

	for i in {1..10};do
		runsshcmd gen-l-vrt-440-162 date && sleep 8
	done

}

function mul_config_ip() {

	sshpass -p 3tango ssh root@gen-l-vrt-440-162 systemctl stop NetworkManager
	sshpass -p 3tango ssh root@gen-l-vrt-440-163 systemctl stop NetworkManager
	sshpass -p 3tango ssh root@gen-l-vrt-440-164 systemctl stop NetworkManager
	sshpass -p 3tango ssh root@gen-l-vrt-440-165 systemctl stop NetworkManager

	sshpass -p 3tango ssh root@gen-l-vrt-440-162 ifconfig eth0 1.1.1.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-162 ifconfig eth1 1.1.2.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-162 ifconfig eth2 1.1.3.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-162 ifconfig eth3 1.1.4.2/24

	sshpass -p 3tango ssh root@gen-l-vrt-440-163 ifconfig eth0 1.1.5.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-163 ifconfig eth1 1.1.6.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-163 ifconfig eth2 1.1.7.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-163 ifconfig eth3 1.1.8.2/24

	sshpass -p 3tango ssh root@gen-l-vrt-440-164 ifconfig eth0 1.1.9.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-164 ifconfig eth1 1.1.10.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-164 ifconfig eth2 1.1.11.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-164 ifconfig eth3 1.1.12.2/24

	sshpass -p 3tango ssh root@gen-l-vrt-440-165 ifconfig eth0 1.1.13.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-165 ifconfig eth1 1.1.14.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-165 ifconfig eth2 1.1.15.2/24
	sshpass -p 3tango ssh root@gen-l-vrt-440-165 ifconfig eth3 1.1.16.2/24

	ifconfig enp59s0f0 0

	ip addr add 1.1.1.1/24 dev enp59s0f0
	ip addr add 1.1.2.1/24 dev enp59s0f0
	ip addr add 1.1.3.1/24 dev enp59s0f0
	ip addr add 1.1.4.1/24 dev enp59s0f0
	ip addr add 1.1.5.1/24 dev enp59s0f0
	ip addr add 1.1.6.1/24 dev enp59s0f0
	ip addr add 1.1.7.1/24 dev enp59s0f0
	ip addr add 1.1.8.1/24 dev enp59s0f0
	ip addr add 1.1.9.1/24 dev enp59s0f0
	ip addr add 1.1.10.1/24 dev enp59s0f0
	ip addr add 1.1.11.1/24 dev enp59s0f0
	ip addr add 1.1.12.1/24 dev enp59s0f0
	ip addr add 1.1.13.1/24 dev enp59s0f0
	ip addr add 1.1.14.1/24 dev enp59s0f0
	ip addr add 1.1.15.1/24 dev enp59s0f0
	ip addr add 1.1.16.1/24 dev enp59s0f0

}

function start_vdpa_multi_dev_vm() {
	stop_vdpa
	start_vdpa

	mul_add_pfs
	mul_add_vfs

	mul_create_vms
	mul_config_ip
}

function shutdown_mul_vm() {
	virsh shutdown gen-l-vrt-440-162-CentOS-7.4
	virsh shutdown gen-l-vrt-440-163-CentOS-7.4
	virsh shutdown gen-l-vrt-440-164-CentOS-7.4
	virsh shutdown gen-l-vrt-440-165-CentOS-7.4
}

function stop_mul_vdpa_vm() {
	stop_vdpa
	shutdown_mul_vm
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

