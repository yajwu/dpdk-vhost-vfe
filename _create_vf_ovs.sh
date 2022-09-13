#!/bin/bash


#mlxconfig -d /dev/mst/mt41686_pciconf0 s \
#VIRTIO_NET_EMULATION_ENABLE=1 VIRTIO_NET_EMULATION_NUM_PF=1 VIRTIO_NET_EMULATION_NUM_VF=16 \
#VIRTIO_BLK_EMULATION_ENABLE=1 VIRTIO_BLK_EMULATION_NUM_PF=1 VIRTIO_BLK_EMULATION_NUM_VF=16 \
#VIRTIO_NET_EMULATION_NUM_MSIX=64 VIRTIO_BLK_EMULATION_NUM_MSIX=64  NUM_VF_MSIX=64

(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && . configs/conf.sh && . common.sh


# 1. bind pf device to vfio
modprobe vfio vfio-pci
echo 0x1af4 0x1041 > /sys/bus/pci/drivers/vfio-pci/new_id 2>/dev/null
echo 0x1af4 0x1042 > /sys/bus/pci/drivers/vfio-pci/new_id 2>/dev/null

echo 1 > /sys/module/vfio_pci/parameters/enable_sriov


for pf in $netpf $blkpf; do
	drv=`readlink -f  /sys/bus/pci/devices/${pf}/driver`
	[[ "$drv" == *"vfio"* ]] || {
		[[ -L /sys/bus/pci/devices/${pf}/driver ]] && {
			echo unbind $pf
			echo $pf > /sys/bus/pci/devices/${pf}/driver/unbind
		}
		echo bind $pf to vfio
		echo $pf > /sys/bus/pci/drivers/vfio-pci/bind
	}
done

#find /sys/bus/pci/drivers/vfio-pci/ -type l

# 2. create vf through vfio sriov
loginfo create vf
echo $numvfs > /sys/bus/pci/devices/${netpf}/sriov_numvfs
loginfo net vfs: `cat /sys/bus/pci/devices/${netpf}/sriov_numvfs`
echo $numvfs > /sys/bus/pci/devices/${blkpf}/sriov_numvfs
loginfo blk vfs: `cat /sys/bus/pci/devices/${blkpf}/sriov_numvfs`

allnetvfs=()
allblkvfs=()
for i in $(seq 0 $(($numvfs - 1)) ); do
	netbdf=`readlink  /sys/bus/pci/devices/${netpf}/virtfn${i}`
	blkbdf=`readlink  /sys/bus/pci/devices/${blkpf}/virtfn${i}`
	allnetvfs+=(`basename $netbdf`)
	allblkvfs+=(`basename $blkbdf`)
	#echo "python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a `basename $netbdf` -v /tmp/vfe-net${i}"
	#echo "python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a `basename $blkbdf` -v /tmp/vfe-blk${i}"
done

loginfo netvfs: ${allnetvfs[@]}
loginfo blkvfs: ${allblkvfs[@]}

export netvf0=${allnetvfs[0]}
export blkvf0=${allblkvfs[0]}
export allnetvfs
export allblkvfs
loginfo netvf0: $netvf0 blkvf0: $blkvf0

# 3. setup ovs on arm for net
#runbf2cmd $bf2ip ovs-vsctl add-port ovsbr1 en3f0pf0sf3000
#runbf2cmd $bf2ip virtnet modify -p 0 -v 0 device -m $devmac
echo p0ethi $p0eth
ip addr add $p0ip/24 dev $p0eth


