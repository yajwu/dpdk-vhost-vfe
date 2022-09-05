#!/bin/bash


function repo_update() {
	echo "#### update snap-rdma ###"
	pushd snap-rdma
	git checkout main
	git pull
	git clean -xdf .
	git status 
	popd

	echo "#### update nvmx ###"
	pushd nvmx
	git checkout main
	git pull
	git clean -xdf .
	git status 
	popd

	echo "#### update virtio-net-controller ###"
	pushd virtio-net-controller
	git checkout v1.4
	git pull
	git clean -xdf .
	git status 
	popd
}


function build_all() {
	echo "#### build virtio-net-controller ###"
	pushd snap-rdma
	./autogen.sh
	./configure --prefix=/usr --libdir=/usr/lib64 --includedir=/usr/include && make -j 8 && make install
	popd

	echo "#### build virtio-net-controller ###"
	pushd nvmx
	./autogen.sh
	./configure  --prefix=/usr --with-spdk=/opt/mellanox/spdk --with-snap=/usr/lib64/ && make -j 8 && make install
	popd

	echo "#### build virtio-net-controller ###"
	pushd virtio-net-controller
	./autogen.sh
	./configure --prefix=/usr --libdir=/usr/lib64 && make -j 8 && make install
	popd
}

cd /root/upstream
repo_update
build_all

date

echo
ls -l /root/upstream/snap-rdma/src/.libs/libsnap.so*
echo
ls -l /root/upstream/nvmx/samples/mlnx_snap_emu
echo
ls -l /root/upstream/virtio-net-controller/controller/virtio_net_controller




