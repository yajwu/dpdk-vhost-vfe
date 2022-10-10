#!/bin/bash

. configs/conf.sh

function update_build_dpdk {
	pushd sw/dpdk

	git checkout main
	git pull
	git clean -xdf .
	cp ../../configs/build_dpdk.sh .

	rm -rf build
	./build_dpdk.sh
	popd
}

function update_build_bf2 {
	sshpass -p centos ssh root@$bf2ip /root/upstream/build.sh

}

update_build_dpdk
update_build_bf2
