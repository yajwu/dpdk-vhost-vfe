#!/bin/bash

shopt -s expand_aliases
source /mswg/projects/fw/fw_ver/hca_fw_tools/.fwvalias


flq /dev/mst/mt41686_pciconf0  | grep "FW Version"
echo "snap-rdma: "
sshpass -p centos ssh root@gen-l-vrt-317-bf "cd /root/upstream/snap-rdma; git rlog -1"
echo -e "\nnvmx: "
sshpass -p centos ssh root@gen-l-vrt-317-bf "cd /root/upstream/nvmx; git rlog -1"
echo -e "\nvirtio-net-controller: "
sshpass -p centos ssh root@gen-l-vrt-317-bf "cd /root/upstream/virtio-net-controller; git rlog -1"
echo -e "\ndpdk: "
pushd sw/dpdk 1>/dev/null && git rlog -1 && popd 1>/dev/null
echo "qemu: "
pushd sw/qemu 1>/dev/null && git rlog -1 && popd 1>/dev/null

