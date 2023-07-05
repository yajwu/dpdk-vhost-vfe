#!/bin/bash

shopt -s expand_aliases

source /mswg/projects/fw/fw_ver/hca_fw_tools/.fwvalias
. configs/conf.sh

flq /dev/mst/mt41686_pciconf0  | grep "FW Version"
echo -n "bfb:   "
sshpass -p centos ssh root@$bf2ip "cat /etc/mlnx-release"
echo -n -e "snap-rdma:   "
sshpass -p centos ssh root@$bf2ip "cd /root/upstream/snap-rdma; git log --pretty=format:\"%h %ad\" --date=short -1"
echo -n -e "\nnvmx:      "
sshpass -p centos ssh root@$bf2ip "cd /root/upstream/nvmx;  git log --pretty=format:\"%h %ad\" --date=short -1"
echo -n -e "\nvirtio-net-controller: "
sshpass -p centos ssh root@$bf2ip "cd /root/upstream/virtio-net-controller;  git log --pretty=format:\"%h %ad\" --date=short -1"
dpdkver=`git -C /images/testvfe/sw/dpdk log --pretty=format:"%h %ad" --date=short -1`
qemuver=`git -C /images/testvfe/sw/qemu log --pretty=format:"%h %ad" --date=short -1`
echo -n -e "\ndpdk: $dpdkver"
echo -n -e "\nqemu: $qemuver"
echo



echo -e "\n == files ==" >&2
ls -l /images/testvfe/sw/dpdk/build/app/dpdk-vfe-vdpa >&2
sshpass -p centos ssh root@$bf2ip ls -l /usr/lib64/libsnap.so.0.0.0 >&2
sshpass -p centos ssh root@$bf2ip ls -l /usr/bin/mlnx_snap_emu >&2
sshpass -p centos ssh root@$bf2ip ls -l /usr/sbin/virtio_net_controller >&2


