obj-m += multi-flow-device-driver.o
multi-flow-device-driver-objs := multi-flow-device.o flow-manager.o
KDIR = /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

show-devices:
	cat /sys/module/multi_flow_device_driver/parameters/enabled

show-hp-bytes:
	cat /sys/module/multi_flow_device_driver/parameters/bytes_in_buffer | cut -d , -f 129-

show-lp-bytes:
	cat /sys/module/multi_flow_device_driver/parameters/bytes_in_buffer | cut -d , -f 1-128

show-hp-threads:
	cat /sys/module/multi_flow_device_driver/parameters/threads_in_wait | cut -d , -f 129-

show-lp-threads:
	cat /sys/module/multi_flow_device_driver/parameters/threads_in_wait | cut -d , -f 1-128