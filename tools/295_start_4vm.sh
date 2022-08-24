#!/bin/bash

python sw/dpdk/app/vfe-vdpa/vhostmgmt mgmtpf -a 0000:af:00.2
sleep 5

sshpass -p centos ssh root@192.168.100.2 virtnet modify -p 0 -v 0 device -m 00:00:00:00:33:00 -f 0x2300471829
sshpass -p centos ssh root@192.168.100.2 virtnet modify -p 0 -v 1 device -m 00:00:00:00:33:01 -f 0x2300471829
sshpass -p centos ssh root@192.168.100.2 virtnet modify -p 0 -v 2 device -m 00:00:00:00:33:02 -f 0x2300471829
sshpass -p centos ssh root@192.168.100.2 virtnet modify -p 0 -v 3 device -m 00:00:00:00:33:03 -f 0x2300471829

#sshpass -p centos ssh root@192.168.100.2 virtnet modify -p 0 -v 0 device -m 00:00:00:00:33:00
#sshpass -p centos ssh root@192.168.100.2 virtnet modify -p 0 -v 1 device -m 00:00:00:00:33:01
#sshpass -p centos ssh root@192.168.100.2 virtnet modify -p 0 -v 2 device -m 00:00:00:00:33:02
#sshpass -p centos ssh root@192.168.100.2 virtnet modify -p 0 -v 3 device -m 00:00:00:00:33:03 


python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:af:04.4
sleep 1
python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:af:04.5
sleep 1
python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:af:04.6
sleep 1
python sw/dpdk/app/vfe-vdpa/vhostmgmt vf -a 0000:af:04.7
sleep 1

sudo virsh create configs/gen-l-vrt-295-005-CentOS-7.4.xml
sudo virsh create configs/gen-l-vrt-295-006-CentOS-7.4.xml
sudo virsh create configs/gen-l-vrt-295-007-CentOS-7.4.xml
sudo virsh create configs/gen-l-vrt-295-008-CentOS-7.4.xml

sleep 30

for i in {1..10};do
	sshpass -p 3tango ssh root@gen-l-vrt-295-005 date
	[[ $? -eq 0 ]] && break
	echo . && sleep 8
done

set -x

sshpass -p 3tango ssh root@gen-l-vrt-295-005 /root/set_005.sh
sshpass -p 3tango ssh root@gen-l-vrt-295-006 /root/set_006.sh
sshpass -p 3tango ssh root@gen-l-vrt-295-007 /root/set_007.sh
sshpass -p 3tango ssh root@gen-l-vrt-295-008 /root/set_008.sh

