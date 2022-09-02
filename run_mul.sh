#!/bin/bash

[ "$EUID" -ne 0 ] && echo "Please run as root" && exit 1

source ./configs/conf.sh
source common.sh
source /mswg/projects/fw/fw_ver/hca_fw_tools/.fwvalias
source _prep_test.sh

#set -x
function run_main {
	export testtype=mul
	[[ -d $logdir/testnet ]] || mkdir $logdir/testnet

	init_cleanup_env
	info_env
	prep_vf_ovs

	export pfslot=$netpf
	export vfslot=$netvf0

	start_vdpa_multi_dev_vm

# run case
	[ -z $cases ] && cases=`ls testmul/*`

	for tc in $cases; do
		export testlog=$logdir/$tc.log
		. ./$tc
		[ -n "$its_overwrite" ] && testits=$its_overwrite
		loginfo "$tc begins with total iteration $testits"

		testcase_pre
		ret="pass"
		for it in `seq $testits`; do
			loginfo "$tc iteration $it / $testits"
			testcase_run
			if [ $? -ne 0 ]; then
				logerr " !!!! test $tc run failed with $? !!!!"
				ret="fail"
				break
			fi
			testcase_check
			if [ $? -ne 0 ]; then
				logerr " !!!! test $tc failed with $? !!!!"
				ret="fail"
				break
			fi
		done

		echo "$tc $testits $ret" >> $testresult
		testcase_clean
		[[ "$ret" == "fail" && "$stop_on_error" == "yes" ]] && break
	done

	stop_mul_vdpa_vm
	post_cleanup_env

	loginfo " ==  result == "
	cat $testresult
}

run_main $1
