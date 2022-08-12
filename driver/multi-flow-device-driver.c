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
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_ioctl(struct file *, unsigned int, unsigned long);

/* Driver operations
*  Each field corresponds to the address of some function defined by the driver to handle a requested operation:
        - open session for a minor
        - release session for a minor
        - read for a minor
        - write for a minor
        - manage I/O control requests for a minor
*/
static struct file_operations fops = {
        .owner = THIS_MODULE,
        .open =  device_open,
        .release = device_release,
        //.write = device_write,
        //.read = device_read,
        .unlocked_ioctl = device_ioctl
};



/**
 * device_open - session opening for a minor
 * @inode:      I/O metadata of the device file
 * @file:       I/O session to the device file
 */
static int device_open(struct inode *inode, struct file *file) {
        int minor;
        session_t *session;

        minor = get_minor(file);
        if (minor >= MINOR_NUMBER) {
                return -ENODEV;
        }
        pr_info("Session opened for minor: %d\n", minor);

        session = kmalloc(sizeof(session_t), GFP_KERNEL);
        session->priority = HIGH_PRIORITY;
        session->flags = GFP_KERNEL;
        session->timeout = MAX_SECONDS;

        file->private_data = session;
        return 0;
}

/**
 * device_release - close session for a minor
 * @inode:      I/O metadata of the device file
 * @file:       I/O session to the device file
 */
static int device_release(struct inode *inode, struct file *file) {
        int minor = get_minor(file);
        pr_info("Session closed for minor: %d\n", minor);
        kfree(file->private_data);
        file->private_data = NULL;
        return 0;
}

/**
 * device_ioctl - manager of I/O control requests 
 * @filp:       I/O session to the device file
 * @command:    requested ioctl command
 * @param:      optional parameter (timeout)
 */
static ssize_t device_ioctl(struct file *filp, unsigned int command, unsigned long param) {
        session_t *session = (session_t *)filp->private_data;
        int minor = get_minor(filp);

        switch (command) {
        case TO_HIGH_PRIORITY:
                session->priority = HIGH_PRIORITY;
                pr_info("Switched to high priority for minor: %d\n", minor);
                break;
        case TO_LOW_PRIORITY:
                session->priority = LOW_PRIORITY;
                pr_info("Switched to low priority for minor: %d\n", minor);
                break;
        case BLOCK:
                session->flags = GFP_KERNEL;
                pr_info("Switched to blocking operations for minor: %d\n", minor);
                break;
        case UNBLOCK:
                session->flags = GFP_ATOMIC;
                pr_info("Switched to unblocking operations for minor: %d\n", minor);
                break;
        case TIMEOUT:
                session->flags = GFP_ATOMIC;
                session->timeout = get_seconds(param);
                pr_info("Setup of timeout for blocking operations to %ld sec for minor: %d\n", session->timeout, minor);
                break;
        default:
                return -ENOTTY;
        }

        return 0;
}

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
