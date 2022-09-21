#!/bin/bash

if lsmod | grep "multi_flow_device_driver" &> /dev/null ; then
    echo "Module already loaded, start re-installation...\n"
    sudo rm /dev/multi_flow_device_*
    sudo rmmod multi_flow_device_driver.ko
    make all
    sudo insmod multi_flow_device_driver.ko
    make clean
    echo "Module loaded!\n"

else
    echo "Module not loaded, start installation...\n"
    make all
    sudo insmod multi_flow_device_driver.ko
    make clean
    echo "Module loaded!\n"
fi