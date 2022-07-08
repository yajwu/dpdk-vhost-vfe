#!/bin/bash


it=0
while true; do
	sleep 0.5
	dd if=/dev/vda of=/dev/null bs=4k count=9999
	if [[ $? -eq  0 ]]; then 
		echo seq $it
	else
		echo dd failed with $? 
		break
	fi
	it=$(( $it + 1 ))
done
