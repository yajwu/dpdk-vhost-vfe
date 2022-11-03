
(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && . configs/conf.sh && . common.sh && export testlog=logs/testlog

.  test_common.sh

export testits=5


function testcase_pre {
	if [[ $numvfs -le 1 ]]; then
		logerr "numvfs is $numvfs, can't run multi device test"
		return 1
	fi
}

function testcase_run {
	shutdown_mul_vm
	runcmd sleep 10

	mul_create_vms
	mul_config_ip

	return 0
}

function testcase_check {
	ping_dd_multi_check
	if [[ $? -ne 0 ]]; then

	loginfo sshpass -p 3tango ssh root@gen-l-vrt-440-162 ifconfig 
	loginfo sshpass -p 3tango ssh root@gen-l-vrt-440-163 ifconfig
	loginfo sshpass -p 3tango ssh root@gen-l-vrt-440-164 ifconfig
	loginfo sshpass -p 3tango ssh root@gen-l-vrt-440-165 ifconfig
	fi
}

function testcase_clean {
:
}



if [ $sourced -eq 0 ]; then
	test_main
fi
