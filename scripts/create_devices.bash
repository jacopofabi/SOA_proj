#!/bin/bash

# if less than one argument supplied, display usage 
if [ $# -ne 1 ] 
then 
    echo "Usage: missing number of devices to create\n"
    exit 1
fi

# if number is less than 1 or greater than 128, exit
if [ $1 -lt 1 -o $1 -gt 128 ]
then
	echo "The driver can create from 1 to 128 devices like the minor number.\n"
	exit 1
fi

base="/dev/multi_flow_device_"
major=$(cat /proc/devices | grep multi-flow | cut -d " " -f 1)

#create all device files
for (( minor=0; minor<$1; minor++ ))
do
  device="$base$minor"
  sudo mknod -m 666 $device c $major $minor
  echo "Created device: $device"
done