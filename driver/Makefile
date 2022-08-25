obj-m += multi-flow-device-driver.o
multi-flow-device-driver-objs := multi-flow-device.o device-manager.o
KDIR = /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
