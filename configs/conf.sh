## bf2 vfe-vdpa test configuration

export stop_on_error=yes
#export its_overwrite=1

export dpdkapp=sw/dpdk/build/examples/dpdk-vdpa
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
export hname=`hostname`

#export cases="testcase_suspend_resume.sh"

if   [ `hostname` == "gen-l-vrt-440" ]; then
	export bf2ip=gen-l-vrt-317-bf
	export p0ip=1.1.3.6

	export vmname=gen-l-vrt-440-162-CentOS-7.4
	export vmxml=${vmname}.xml
	export vmip=gen-l-vrt-440-162
	export devmac="e4:11:c6:d3:45:f4"
	export vmethip=1.1.3.20

	export peer=gen-l-vrt-439
fi

