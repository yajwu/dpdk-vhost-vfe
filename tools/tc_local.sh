tc qdisc del dev pf0hpf ingress 
tc qdisc add dev pf0hpf ingress 

tc qdisc del dev p0 ingress 
tc qdisc add dev p0 ingress 

tc qdisc del dev en3f0pf0sf3000 ingress 
tc qdisc add dev en3f0pf0sf3000 ingress 

tc qdisc del dev en3f0pf0sf3001 ingress 
tc qdisc add dev en3f0pf0sf3001 ingress

tc qdisc del dev en3f0pf0sf3002 ingress 
tc qdisc add dev en3f0pf0sf3002 ingress

tc qdisc del dev en3f0pf0sf3003 ingress 
tc qdisc add dev en3f0pf0sf3003 ingress

tc filter add dev p0 protocol ip  ingress prio 3 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev p0 protocol arp ingress prio 2 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev p0 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev p0 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev p0 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev p0 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev p0 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev p0 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev p0 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003
tc filter add dev p0 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003


tc filter add dev pf0hpf protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev pf0hpf protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev pf0hpf protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev pf0hpf protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev pf0hpf protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev pf0hpf protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev pf0hpf protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003
tc filter add dev pf0hpf protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003


tc filter add dev en3f0pf0sf3000 protocol ip  ingress prio 3 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev en3f0pf0sf3000 protocol arp ingress prio 2 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev en3f0pf0sf3000 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev en3f0pf0sf3000 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev en3f0pf0sf3000 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev en3f0pf0sf3000 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev en3f0pf0sf3000 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003
tc filter add dev en3f0pf0sf3000 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003

tc filter add dev en3f0pf0sf3001 protocol ip  ingress prio 3 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev en3f0pf0sf3001 protocol arp ingress prio 2 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev en3f0pf0sf3001 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev en3f0pf0sf3001 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev en3f0pf0sf3001 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev en3f0pf0sf3001 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev en3f0pf0sf3001 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003
tc filter add dev en3f0pf0sf3001 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003

tc filter add dev en3f0pf0sf3002 protocol ip  ingress prio 3 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev en3f0pf0sf3002 protocol arp ingress prio 2 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev en3f0pf0sf3002 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev en3f0pf0sf3002 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev en3f0pf0sf3002 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev en3f0pf0sf3002 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev en3f0pf0sf3002 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003
tc filter add dev en3f0pf0sf3002 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:03 action mirred egress redirect dev en3f0pf0sf3003


tc filter add dev en3f0pf0sf3003 protocol ip  ingress prio 3 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev en3f0pf0sf3003 protocol arp ingress prio 2 flower dst_mac 08:c0:eb:b2:85:ac action mirred egress redirect dev pf0hpf
tc filter add dev en3f0pf0sf3003 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev en3f0pf0sf3003 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:00 action mirred egress redirect dev en3f0pf0sf3000
tc filter add dev en3f0pf0sf3003 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev en3f0pf0sf3003 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:01 action mirred egress redirect dev en3f0pf0sf3001
tc filter add dev en3f0pf0sf3003 protocol ip  ingress prio 3 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002
tc filter add dev en3f0pf0sf3003 protocol arp ingress prio 2 flower dst_mac 00:00:00:00:33:02 action mirred egress redirect dev en3f0pf0sf3002



