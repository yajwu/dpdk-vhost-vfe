#!/bin/bash

function add_vfs() {

python sw/dpdk/app/vfe-vdpa/vhostmgmt mgmtpf -a 0000:3b:00.2
sleep 6
python sw/dpdk/app/vfe-vdpa/vhostmgmt mgmtpf -a 0000:3b:00.3
sleep 6

sshpass -p centos ssh root@gen-l-vrt-317-bf  'for i in {0..15}; do snap_rpc.py controller_virtio_blk_create mlx5_0 --pf_id 0 --vf_id $i --bdev_type spdk --bdev Null0; done'
sshpass -p centos ssh root@gen-l-vrt-317-bf  'for i in {00..15}; do  virtnet modify -p 0 -v $i device -m 00:00:04:40:62:${i}; done'

#python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:04.5 -v /tmp/vfe-net0
#python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.5 -v /tmp/vfe-blk0
python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:04.6 -v /tmp/vfe-net1
#python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:3b:06.6 -v /tmp/vfe-blk1

return 0
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

}


function create_vms() {

sudo virsh create configs/4_4/gen-l-vrt-440-162-CentOS-7.4.xml
sudo virsh create configs/4_4/gen-l-vrt-440-163-CentOS-7.4.xml
sudo virsh create configs/4_4/gen-l-vrt-440-164-CentOS-7.4.xml
sudo virsh create configs/4_4/gen-l-vrt-440-165-CentOS-7.4.xml

}


function set_ip() {
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

function check_net() {
for i in {1..16}; do ping 1.1.${i}.2 -I 1.1.${i}.1 -c2 ; done
}

function check_blk() {
for i in {162..165}; do
	sshpass -p 3tango ssh root@gen-l-vrt-440-${i} dd if=/dev/vda of=/dev/null
	sshpass -p 3tango ssh root@gen-l-vrt-440-${i} dd if=/dev/vdb of=/dev/null
	sshpass -p 3tango ssh root@gen-l-vrt-440-${i} dd if=/dev/vdc of=/dev/null
	sshpass -p 3tango ssh root@gen-l-vrt-440-${i} dd if=/dev/vdd of=/dev/null
done
}


#add_vfs
#create_vms
