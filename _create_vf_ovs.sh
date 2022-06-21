#!/bin/bash


#mlxconfig -d /dev/mst/mt41686_pciconf0 s VIRTIO_NET_EMULATION_ENABLE=1 VIRTIO_NET_EMULATION_NUM_PF=1 \
#VIRTIO_BLK_EMULATION_ENABLE=1 VIRTIO_BLK_EMULATION_NUM_VF=4 VIRTIO_NET_EMULATION_NUM_VF=4 \
#VIRTIO_NET_EMULATION_NUM_MSIX=8 VIRTIO_BLK_EMULATION_NUM_MSIX=8 VIRTIO_BLK_EMULATION_NUM_PF=1

. configs/conf.sh
. common.sh


# 1. bind pf device to vfio
sudo modprobe vfio vfio-pci
echo 0x1af4 0x1041 > /sys/bus/pci/drivers/vfio-pci/new_id 2>/dev/null
echo 0x1af4 0x1042 > /sys/bus/pci/drivers/vfio-pci/new_id 2>/dev/null

echo 1 > /sys/module/vfio_pci/parameters/enable_sriov


for pf in $netpf $blkpf; do
	drv=`readlink -f  /sys/bus/pci/devices/${pf}/driver`
	echo $drv
	[[ "$drv" == *"vfio"* ]] || {
		[[ -L /sys/bus/pci/devices/driver ]] && {
			echo unbind $pf
			echo $pf > /sys/bus/pci/devices/driver/unbind
		}
		echo bind $pf to vfio
		echo $pf > /sys/bus/pci/drivers/vfio-pci/bind
	}
done

#find /sys/bus/pci/drivers/vfio-pci/ -type l


# 2. create vf through vfio sriov
loginfo create vf
echo 1 > /sys/bus/pci/devices/${netpf}/sriov_numvfs
loginfo net vfs: `cat /sys/bus/pci/devices/${netpf}/sriov_numvfs`
echo 1 > /sys/bus/pci/devices/${blkpf}/sriov_numvfs
loginfo blk vfs: `cat /sys/bus/pci/devices/${blkpf}/sriov_numvfs`

# 3. setup ovs on arm for net
#runbf2cmd $bf2ip ovs-vsctl add-port ovsbr1 en3f0pf0sf3000 
runbf2cmd $bf2ip virtnet modify -p 0 -v 0 device -m $devmac

