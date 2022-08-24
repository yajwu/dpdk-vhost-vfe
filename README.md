# auto-test for vfe-vdpa

## all in one test, must be root

	# run net all test
	sudo ./run_net.sh

	# run blk all test
	sudo ./run_blk.sh

## run single test

	## !! change cases in it
	sudo ./run_net_single.sh

## configuration

	configs/config.sh

## log

	# in latest/
	tail -f latest/vdpa.log

## sw

	sw/dpdk
	sw/qemu

