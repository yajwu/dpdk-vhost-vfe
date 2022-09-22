
function wait_vm {
	runsshcmd $vmip date && return 0

	sleep 15
	for i in {1..10};do
		runsshcmd $vmip date
		[[ $? -eq 0 ]] && break
		echo . && sleep 8
	done
}

function ping_pre {
	loginfo wait vm bootup ...

	wait_vm
	runsshcmd $vmip systemctl stop NetworkManager
	runsshcmd $vmip ifconfig $vmeth ${vmethip}/24
	runsshcmd $vmip ifconfig $vmeth
	runsshcmd $vmip ethtool -l $vmeth

	#runsshcmd_bg "$vmip ping $testbed_br_ip -i 0.6 |tee -a $testlog"
	runcmd_bg "ping $vmethip -i 0.5 |tee -a $testlog"
	export ping_pid=$!
	sleep 2
}

function ping_pre_without_start {
	loginfo wait vm bootup ...

	wait_vm
	runsshcmd $vmip systemctl stop NetworkManager
	runsshcmd $vmip ifconfig $vmeth ${vmethip}/24
	runsshcmd $vmip ifconfig $vmeth
	runsshcmd $vmip ethtool -l $vmeth
	sleep 2
}

function ping_check {
	local seq1=`tail -n 1 $testlog | egrep -o 'icmp_seq=.*time=' | cut -d ' '  -f 1 | cut -d "=" -f 2`
	sleep 4
	local seq2=`tail -n 1 $testlog | egrep -o 'icmp_seq=.*time=' | cut -d ' '  -f 1 | cut -d "=" -f 2`
	if [[ $seq2 -le $seq1 ]]; then
		logerr "ping check fail: $seq1 vs $seq2"
		return 1
	else
		loginfo " $seq2 vs $seq1, ping seq increase. check pass"
		return 0
	fi
}

function dd_check {
	local seq1=`tail -n 1 $testlog | egrep 'seq' | cut -d " " -f 2`
	sleep 2
	local seq2=`tail -n 1 $testlog | egrep 'seq' | cut -d " " -f 2`
	loginfo seq1 $seq1 seq2 $seq2
	if [[ $seq2 -le $seq1 ]]; then
		logerr "dd check fail: $seq1 vs $seq2"
		return 1
	else
		loginfo " $seq2 vs $seq1, dd seq increase. check pass"
		return 0
	fi

}

function dd_pre {
	loginfo wait vm bootup ...

	wait_vm
	runsshcmd_bg $vmip '/root/vfe_dd_dev.sh | tee -a $testlog'
	sleep 2
}

function vm_check_running {
	local vms=`runsshcmd $1 virsh list --state-running --name`
	runsshcmd $1 virsh list --all
	[[ "$vms" == *"$2"* ]] || { logerr "vm_check_running fail: $1 $2" && return 1; }
}

function vm_check_down {
	local vms=`runsshcmd $1 virsh list --state-shutoff --name`
	runsshcmd $1 virsh list --all
	[[ "$vms" == *"$2"* ]] || { logerr "vm_check_down fail: $1 $2" && return 1 ;}
}

function ping_clean {
	pkill -SIGTERM -x ping
}

function dd_clean {
	pkill -SIGTERM -f vfe_dd_dev.sh
}


function test_main {
	. _prep_test.sh
	testcase_pre
	testcase_run
	echo "testcase_run result $?"
	testcase_check
	echo "testcase_check result $?"
	testcase_clean
}

