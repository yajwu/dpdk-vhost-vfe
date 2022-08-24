#!/bin/bash

[ "$EUID" -ne 0 ] && echo "Please run as root" && exit 1

source ./configs/conf.sh
source common.sh
source /mswg/projects/fw/fw_ver/hca_fw_tools/.fwvalias
source _prep_test.sh
#set -x

function run_main {
	export testtype=blk
	[[ -d $logdir/testblk ]] || mkdir $logdir/testblk

	init_cleanup_env
	info_env
	prep_vf_ovs

	export pfslot=$blkpf
	export vfslot=$blkvf0

	if [ "$1" == "slave" ]; then
		start_vdpa
		exit 0
	else
		start_vdpa_vm
	fi

# run case
	[ -z $cases ] && cases=`ls testblk/*`

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

	post_cleanup_env

	loginfo " ==  result == "
	cat $testresult
}

run_main $1
