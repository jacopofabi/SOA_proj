#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "Usage: missing number of devices to create\n"
fi

base="/dev/multi_flow_device_"
major=$(cat /proc/devices | grep multi-flow | cut -f 1 -d " ")

#create all device files
for (( minor=0; minor<$1; minor++ ))
do
  device="$base$minor"
  echo "Created device: $device"
  sudo mknod -m 666 $device c $major $minor
done
