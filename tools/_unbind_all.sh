#!/bin/bash


#mlxconfig -d /dev/mst/mt41686_pciconf0 s VIRTIO_NET_EMULATION_ENABLE=1 VIRTIO_NET_EMULATION_NUM_PF=1 \
#VIRTIO_BLK_EMULATION_ENABLE=1 VIRTIO_BLK_EMULATION_NUM_VF=4 VIRTIO_NET_EMULATION_NUM_VF=4 \
#VIRTIO_NET_EMULATION_NUM_MSIX=8 VIRTIO_BLK_EMULATION_NUM_MSIX=8 VIRTIO_BLK_EMULATION_NUM_PF=1
(return 0 2>/dev/null) && sourced=1 || sourced=0
[ $sourced -eq 0 ] && . configs/conf.sh && . common.sh


# 1. bind pf device to vfio
modprobe vfio vfio-pci
echo 0x1af4 0x1041 > /sys/bus/pci/drivers/vfio-pci/new_id 2>/dev/null
echo 0x1af4 0x1042 > /sys/bus/pci/drivers/vfio-pci/new_id 2>/dev/null


for pf in $netpf $blkpf; do
	echo unbind $pf
	echo $pf > /sys/bus/pci/devices/${pf}/driver/unbind
done

#find /sys/bus/pci/drivers/vfio-pci/ -type l


