#!/bin/bash

shopt -s expand_aliases
source /mswg/projects/fw/fw_ver/hca_fw_tools/.fwvalias
. configs/conf.sh

echo " == version =="
flq /dev/mst/mt41686_pciconf0  | grep "FW Version"
echo "snap-rdma: "
sshpass -p centos ssh root@$bf2ip "cd /root/upstream/snap-rdma; git rlog -1"
echo -e "\nnvmx: "
sshpass -p centos ssh root@$bf2ip "cd /root/upstream/nvmx; git rlog -1"
echo -e "\nvirtio-net-controller: "
sshpass -p centos ssh root@$bf2ip "cd /root/upstream/virtio-net-controller; git rlog -1"
echo -e "\ndpdk: "
git -C /images/testvfe/sw/dpdk log --no-merges --pretty=format:'%h%d (%ci) %s  <%an>' -1 
echo -e "\nqemu: "
git -C /images/testvfe/sw/qemu log --no-merges --pretty=format:'%h%d (%ci) %s  <%an>' -1


echo -e "\n == files =="
ls -l /images/testvfe/sw/dpdk/build/app/dpdk-vfe-vdpa
sshpass -p centos ssh root@$bf2ip ls -l /usr/lib64/libsnap.so.0.0.0
sshpass -p centos ssh root@$bf2ip ls -l /usr/bin/mlnx_snap_emu
sshpass -p centos ssh root@$bf2ip ls -l /usr/sbin/virtio_net_controller


