#!/bin/bash

# if less than two argument supplied, display usage 
if [ $# -ne 2 ] 
then 
    echo "Usage: specify minor number and Y/N to enable/disable the device\n"
    exit 1
fi

# if minor number is less than 0 or greater than 127, exit
if [ $1 -lt 0 -o $1 -gt 127 ]
then
	echo "The minor must be a number between 0 to 127."
	exit 1
fi

# if enable/disable is not Y or N, exit
if [ $2 != 'Y' -a $2 != 'N' ] 
then
	echo "The enabled flag must be Y or N."
	exit 1;
fi

# device file index start from 1 instead of 0 after awk
index=$(($1+1))

# copy the enabled list in a temporary file, change the enabled flag for the specific minor and re-copy on the original parameter file
cp /sys/module/multi_flow_device_driver/parameters/enabled /tmp/to_delete
awk 'BEGIN{FS=OFS=","} {if (NR==1) {$'${index}' = "'${2}'"}; print}' /tmp/to_delete > /sys/module/multi_flow_device_driver/parameters/enabled
rm /tmp/to_delete
echo "Operation completed."
