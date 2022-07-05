function wait_vm {
	sleep 10
	for i in {1..10};do
		runsshcmd $vmip date
		[[ $? -eq 0 ]] && break
		echo . && sleep 6
	done
}

function ping_pre {
	loginfo wait vm bootup ...

	wait_vm
	runsshcmd $vmip systemctl stop NetworkManager
	runsshcmd $vmip ifconfig $vmeth $vmethip
	runsshcmd $vmip ifconfig $vmeth
	runsshcmd $vmip ethtool -l $vmeth

	#runsshcmd_bg "$vmip ping $testbed_br_ip -i 0.6 |tee -a $testlog"
	runcmd_bg "ping $vmethip -i 0.6 |tee -a $testlog"
	export ping_pid=$!
	sleep 2
}

function ping_check {
	local seq1=`tail -n 1 $testlog | egrep -o 'icmp_seq=.*time=' | cut -d ' '  -f 1 | cut -d "=" -f 2`
	sleep 2
	local seq2=`tail -n 1 $testlog | egrep -o 'icmp_seq=.*time=' | cut -d ' '  -f 1 | cut -d "=" -f 2`
	if [[ $seq2 -le $seq1 ]]; then
		logerr "ping check fail: $seq1 vs $seq2"
		return 1
	else
		loginfo " $seq2 vs $seq1, ping seq increase. check pass"
		return 0
	fi
}

function vm_check_running {
	local vms=`runsshcmd $1 virsh list --state-running --name`
	runsshcmd $1 virsh list --all
	[[ "$vms" == *"$2"* ]] || ( logerr "vm_check_running fail: $1 $2" && return 1)
}

function vm_check_down {
	local vms=`runsshcmd $1 virsh list --state-shutoff --name`
	runsshcmd $1 virsh list --all
	[[ "$vms" == *"$2"* ]] || ( logerr "vm_check_down fail: $1 $2" && return 1)
}

function ping_clean {
	pkill -SIGTERM -x ping
}

function test_main {
	testcase_pre
	testcase_run
	echo "testcase_run result $?"
	testcase_check
	echo "testcase_check result $?"
	testcase_clean
}

