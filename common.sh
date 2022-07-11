shopt -s direxpand
shopt -s expand_aliases

[ -n "$logfile" ] && exit 0

function loginfo {
    local cmd=$*
    echo -e "\n[ $(date "+%F-%H-%M-%S") $hname ]" $cmd
	echo
}

function logerr {
    local cmd=$*
    echo -e "\n[" `date` $hname =error= "]" $cmd
	echo
}

function runcmd {
    local cmd=$*
    loginfo "run: " $cmd
    eval $cmd
}

function runcmd_bg {
    local cmd=$*
    loginfo "run bg: " $cmd
    eval $cmd &
}

function runsshcmd {
	local serip=$1
	shift
    local cmd=$*
    loginfo "run ssh: " $serip $cmd
    eval sshpass -p 3tango ssh root@$serip "$cmd"
}

function runsshcmd_bg {
	local serip=$1
	shift
    local cmd=$*
    loginfo "run ssh bg: " $serip $cmd
    eval sshpass -p 3tango ssh root@$serip $cmd &
}

function runbf2cmd {
	local serip=$1
	shift
    local cmd=$*
    loginfo "run bf2 ssh: " $serip $cmd
    eval sshpass -p centos ssh root@$serip "$cmd"
}


timestamp=$(date +"%Y-%m-%d_%H_%M")

export logdir=`pwd`/logs/${timestamp}
export logfile=${logdir}/$(basename ${0}).log

ln -nsf $logdir latest

[ -d logs ] || mkdir logs
[ -d $logdir ] || mkdir -p $logdir

exec &> >(tee  $logfile)


