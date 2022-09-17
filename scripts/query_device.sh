#!/bin/bash

# if less than one argument supplied, display usage 
if [ $# -ne 1 ] 
then 
    echo "Usage: specify minor number to query a specific device\n"
    exit 1
fi

# if minor number is less than 0 or greater than 127, exit
if [ $1 -lt 0 -o $1 -gt 127 ]
then
	echo "The minor must be a number between 0 to 127."
	exit 1
fi

# device file index start from 1 instead of 0 after cut
low_index=$(($1+1))
high_index=$((128+$1+1))

echo "Enabled: "
cut -d , -f $low_index /sys/module/multi_flow_device_driver/parameters/enabled

echo "Low priority threads in wait: "
cut -d , -f $low_index /sys/module/multi_flow_device_driver/parameters/threads_in_wait

echo "High priority threads in wait: "
cut -d , -f $high_index /sys/module/multi_flow_device_driver/parameters/threads_in_wait

echo "Low priority bytes in buffer: "
cut -d , -f $low_index /sys/module/multi_flow_device_driver/parameters/bytes_in_buffer

echo "High priority bytes in buffer: "
cut -d , -f $high_index /sys/module/multi_flow_device_driver/parameters/bytes_in_buffer