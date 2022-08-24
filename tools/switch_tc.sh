# !/bin/bash -x


while true; do

sudo virsh event gen-l-vrt-295-005-CentOS-7.4 --event "lifecycle"
sshpass -p centos ssh root@192.168.100.2 /root/setVdpaTcRules_peer.sh

ncat  192.168.100.2 8888 -u << end
peer
end

sudo virsh event gen-l-vrt-295-005-CentOS-7.4 --event "lifecycle"
sudo virsh event gen-l-vrt-295-005-CentOS-7.4 --event "lifecycle"

ncat  192.168.100.2 8888 -u << end
local
end

done

function telnet {

expect << END

spawn telnet 192.168.100.2 -l root
expect "Password: "
send "centos\n"
expect "localhost ~"
send "/root/setVdpaTcRules_peer.sh \n"
expect "localhost ~"

# end of expect script.
END

}
