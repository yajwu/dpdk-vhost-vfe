#!/bin/bash

. configs/conf.sh

function clone_dpdk_mlx {
	pushd sw
	git clone git@github.com:Mellanox/dpdk.org.git -b mlnx_dpdk_20.11_last_stable
	popd
}

function clone_qemu {
	pushd sw
	git clone git@github.com:Mellanox/qemu.git -b mlx_4.2.0 qemu.mlx
	pushd qemu.mlx
	git submodule update --init --recursive
	popd
	popd
}

function clone_qemu_gerrit {
	pushd sw
	git clone "http://l-gerrit.mtl.labs.mlnx:8080/upstream/qemu" qemu.gerrit
	pushd qemu.gerrit
	git fetch "http://l-gerrit.mtl.labs.mlnx:8080/upstream/qemu" refs/changes/56/506856/8 && git checkout FETCH_HEAD
	git submodule update --init --recursive
	popd
	popd
}

function clone_dpdk_vfe {
	pushd sw
	git clone git@github.com:Mellanox/dpdk-vhost-vfe.git
	popd
}

[ -d sw ] || mkdir sw

[ -d sw/dpdk-vhost-vfe ] || clone_dpdk_vfe
[ -d sw/qemu.gerrit ] || clone_qemu_gerrit

[ -L sw/dpdk ] || ln -sf dpdk-vhost-vfe sw/dpdk
[ -L sw/qemu ] || ln -sf qemu.gerrit sw/qemu

if [ ! -f $dpdkapp ]; then
	cp ./configs/build_dpdk.sh sw/dpdk/
	pushd sw/dpdk
	./build_dpdk.sh
	popd
fi

if [ ! -f $qemuapp ]; then
	cp ./configs/build_qemu.sh sw/qemu/
	pushd sw/qemu
	./build_qemu.sh
	popd
fi

pushd sw/dpdk/ && git branch -v && popd
pushd sw/qemu/ && git branch -v && popd

$qemuapp --version

