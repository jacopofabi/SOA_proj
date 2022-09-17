#!/bin/bash

# if there are 0 argument supplied, display usage 
if [ $# -eq 0 ]
  then
    echo "Usage: missing number of devices to create\n"
    exit 1
fi

base="/dev/multi_flow_device_"
major=$(cat /proc/devices | grep multi-flow | cut -d " " -f 1)

#create all device files
for (( minor=0; minor<$1; minor++ ))
do
  device="$base$minor"
  echo "Created device: $device"
  sudo mknod -m 666 $device c $major $minor
done
