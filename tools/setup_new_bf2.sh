#!/bin/bash
set -x

yum-config-manager --add-repo=http://mirror.centos.org/centos/8-stream/AppStream/aarch64/os/
yum install sshpass -y

for i in {3000..3015}; do ovs-vsctl add-port ovsbr1 en3f0pf0sf${i} ; done
echo "bdev_null_create Null0 1024 512" > /etc/mlnx_snap/spdk_rpc_init.conf 
echo "controller_virtio_blk_create --pf_id 0 --bdev_type spdk mlx5_0 --bdev Null0 --num_queues 1 --admin_q --force_in_order" > /etc/mlnx_snap/snap_rpc_init.conf 
systemctl restart mlnx_snap
#systemctl status mlnx_snap -l


timedatectl set-timezone Asia/Shanghai

mkdir upstream; cd upstream
mkdir /root/.ssh
sshpass -p yajunw11 scp yajunw@gen-l-vrt-440:/labhome/yajunw/.ssh/* /root/.ssh/
cp /root/.ssh/config1 /root/.ssh/config

git clone ssh://yajunw@l-gerrit.mtl.labs.mlnx:29418/virtio-net-controller 
git clone git@github.com:Mellanox/nvmx.git
git clone git@github.com:Mellanox/snap-rdma.git

sshpass -p yajunw11 scp yajunw@gen-l-vrt-440:/labhome/yajunw/.gitconfig /root/
sshpass -p 3tango scp  root@gen-l-vrt-440:/images/testvfe/tools/build_bf2.sh build.sh

