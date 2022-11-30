#!/bin/bash

. configs/conf.sh

function clone_dpdk_mlx {
	pushd sw
	git clone git@github.com:Mellanox/dpdk.org.git -b mlnx_dpdk_20.11_last_stable
	popd
}

function clone_qemu {
	pushd sw
	git clone git@github.com:Mellanox/qemu.git -b mlx_vfe_vdpa qemu.mlx
	pushd qemu.mlx
	git submodule update --init --recursive
	popd
	popd
}

function clone_qemu_gerrit {
	pushd sw
	git clone "http://l-gerrit.mtl.labs.mlnx:8080/upstream/qemu" qemu.gerrit
	pushd qemu.gerrit
	git fetch "http://l-gerrit.mtl.labs.mlnx:8080/upstream/qemu" refs/changes/93/510893/5 && git checkout FETCH_HEAD
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
[ -d sw/qemu.mlx ] || clone_qemu

[ -L sw/dpdk ] || ln -sfn dpdk-vhost-vfe sw/dpdk
[ -L sw/qemu ] || ln -sfn qemu.mlx sw/qemu

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

