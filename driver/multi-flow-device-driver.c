/********************************************************************************
*  \file       multi-flow-device-driver.c
*
*  \author     Jacopo Fabi
*
*  \details    Linux multiflow device driver
*
* *******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/version.h>
#include "lib/definitions.h"

/* Module parameters */
bool enabled[MINOR_NUMBER] = {[0 ... (MINOR_NUMBER-1)] = true};                         //state of the device files
long byte_in_buffer[FLOWS * MINOR_NUMBER];                                              //#bytes in buffer for every minor
long thread_in_wait[FLOWS * MINOR_NUMBER];                                              //#threads in wait on buffer for every minor
module_param_array(enabled, bool, NULL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param_array(byte_in_buffer, long, NULL, S_IRUSR | S_IRGRP);
module_param_array(thread_in_wait, long, NULL, S_IRUSR | S_IRGRP);

/* Global variables */
static int major;

/* Function prototypes */
int init_module(void);
void cleanup_module(void);

/**
 * Module initialization function
 */
int init_module(void) {
        printk(KERN_INFO "Welcome!\n");
        printk(KERN_INFO "This is a multiflow device driver implementation of Jacopo Fabi\n");

        // dynamic allocation of major number (minor from 0 to 127)
        // allows to specify the base minor number and the count of minor devices
        // device driver --> /proc/devices ; device files --> /dev
        major = __register_chrdev(0, 0, MINOR_NUMBER, DEVICE_NAME, &fops);

        if (major < 0) {
                printk(KERN_INFO "%s: cannot allocate major number\n", MODNAME);
                return major;
        }

        printk(KERN_INFO "Kernel Module Inserted Successfully...\n");
        printk(KERN_INFO "%s: new driver registered, it is assigned major number %d\n",MODNAME, major);
        return 0;
        }


/**
 * Module cleanup function
 */
void cleanup_module(void) {
        __unregister_chrdev(major, 0, MINOR_NUMBER, DEVICE_NAME);
        printk(KERN_INFO "%s: driver with major number %d unregistered\n",MODNAME, major);
        return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jacopo Fabi <jacopo.fabi1997@gmail.com");
MODULE_DESCRIPTION("Linux device driver that implements low and high priority flows of data");
MODULE_VERSION("1:1.0");
