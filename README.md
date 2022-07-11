# auto-test for vfe-vdpa

## all in one test, must be root

	# run net all test
	sudo ./run_net.sh

	# run blk all test
	sudo ./run_blk.sh

## run single test

	sudo ./_create_vf_ovs.sh
	## !! change testtype pfslot vfslot
	sudo ./_prep_test.sh
	sudo ./testblk_dd_00.sh

## configuration

	configs/config.sh

## log

	# in latest/
	tail -f latest/vdpa.log

## sw

	# in sw
	ln -sfn dpdk-xx dpdk
	ln -sfn qemu-xx qemu

