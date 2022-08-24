## bf2 vfe-vdpa test configuration

export stop_on_error=yes
#export its_overwrite=1

export dpdkapp=sw/dpdk/build/app/dpdk-vfe-vdpa
export qemuapp=sw/qemu/bin/x86_64-softmmu/qemu-system-x86_64
export testtype=net

## default value is on gen-l-vrt-439
export netpf=0000:3b:00.2
export blkpf=0000:3b:00.3
export bf2ip=gen-l-vrt-316-bf
export p0ip=1.1.2.5
export p0eth=enp59s0f0

export vmname=gen-l-vrt-439-141-CentOS-7.4
export vmxml=${vmname}.xml
export vmip=gen-l-vrt-439-141
export vmeth=eth1
export vmethip=1.1.2.20
export devmac="e4:11:c6:d3:45:f3"

export peer=gen-l-vrt-440
export peerbf2=gen-l-vrt-316-bf
export hname=`hostname`

export numvfs=1

#export cases="testcase_suspend_resume.sh"

if   [ `hostname` == "gen-l-vrt-440" ]; then
	export bf2ip=gen-l-vrt-317-bf
	export p0ip=1.2.1.6

	export vmname=gen-l-vrt-440-162-CentOS-7.4
	export vmxml=${vmname}.xml
	export vmip=gen-l-vrt-440-162
	export devmac="e4:11:c6:d3:45:f4"
	export vmethip=1.2.1.20

	export peer=gen-l-vrt-439
	export peerbf2=gen-l-vrt-316-bf

	export numvfs=16
elif [ `hostname` == "gen-l-vrt-292" ]; then
	export netpf=0000:5e:00.2

	export bf2ip=gen-l-vrt-292-bf2
	export p0eth=enp94s0f0
	export p0ip=1.1.6.6

	export vmname=gen-l-vrt-292-005-RH-8.2
	export vmxml=${vmname}.xml
	export vmip=gen-l-vrt-292-005
	export devmac="e4:11:c6:d3:45:55"
	export vmethip=1.1.6.20
	export vmeth=ens7
	export vmqps=4

	export peer=gen-l-vrt-293
	unset peerbf2
elif [ `hostname` == "gen-l-vrt-294" ]; then
	export netpf=0000:af:00.2
	export blkpf=0000:af:00.3

	export bf2ip=gen-l-vrt-294-bf
	export p0ip=1.1.5.6

	#export vmname=gen-l-vrt-292-005-RH-8.2
	#export vmxml=${vmname}.xml
	#export vmip=gen-l-vrt-292-005
	export devmac="00:00:00:00:22:00"
	export vmethip=1.1.5.20
	#export vmeth=ens7

	export peer=gen-l-vrt-295
	unset peerbf2
elif [ `hostname` == "gen-l-vrt-295" ]; then
	export netpf=0000:af:00.2
	export blkpf=0000:af:00.3

	export bf2ip=gen-l-vrt-295-bf
	export p0ip=1.1.7.6

	export vmname=gen-l-vrt-295-005-CentOS-7.4
	export vmxml=${vmname}.xml
	export vmip=gen-l-vrt-292-005
	export devmac="00:00:00:00:33:00"
	export vmethip=1.1.7.20
	export vmeth=eth1

	export peer=gen-l-vrt-294
	export peerbf2=gen-l-vrt-294-bf
fi

