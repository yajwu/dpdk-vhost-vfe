#!/bin/bash


sudo ifconfig tmfifo_net0 192.168.100.1
sudo ifconfig ens4f0np0  1.1.5.5/24

sudo arp -s 1.1.5.10 00:00:00:00:33:00
sudo arp -s 1.1.5.11 00:00:00:00:33:01
sudo arp -s 1.1.5.12 00:00:00:00:33:02
sudo arp -s 1.1.5.13 00:00:00:00:33:03

