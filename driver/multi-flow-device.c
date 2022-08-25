/********************************************************************************
*  \file       multi-flow-device.c
*
*  \author     Jacopo Fabi
*
*  \details    Linux multiflow device driver implementation
*
* *******************************************************************************/
#include "lib/defines.h"

// TODO: setup deferred write for low priority and test
// TODO: see likely/unlikely for branch prediction (se predizione è corretta ho zero cicli di clock, altrimenti so tanti)
// potremmo usarle sfruttando un profilo operazionale (forse)

/* Module parameters */
bool enabled[MINOR_NUMBER] = {[0 ... (MINOR_NUMBER-1)] = true};                         //state of the device files
long byte_in_buffer[FLOWS * MINOR_NUMBER];                                              //#bytes in buffer for every minor
long thread_in_wait[FLOWS * MINOR_NUMBER];                                              //#threads in wait on buffer for every minor
module_param_array(enabled, bool, NULL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param_array(byte_in_buffer, long, NULL, S_IRUSR | S_IRGRP);
module_param_array(thread_in_wait, long, NULL, S_IRUSR | S_IRGRP);

/* Global variables */
static int major;
object_t devices[MINOR_NUMBER];
long booked_byte[MINOR_NUMBER] = {[0 ... (MINOR_NUMBER-1)] = 0};

/* Function prototypes */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
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
        .write = device_write,
        .read = device_read,
        .open =  device_open,
        .release = device_release,
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
 * device_read - read operation for a minor
 * @filp:       I/O session to the device file
 * @buff:       pointer to a buffer that contains read data
 * @len:        number of bytes to be read
 * 
 * Returns:
 *  - # of read bytes when the operation is successful
 *  - a negative value when error occurs
 */
static ssize_t device_read(struct file *filp, char *buff, size_t len, loff_t *off) {
        int res;
        int minor;
        char *tmp_buf;
        object_t *obj;
        session_t *session;
        device_manager_t *buffer;

        minor = get_minor(filp);
        obj = devices + minor;
        session = (session_t *)filp->private_data;
        buffer = obj->buffer[session->priority];

        pr_info("Read operation called for minor: %d\n", minor);

        if (len == 0) return 0;

        tmp_buf = kmalloc(len, session->flags);

        if(is_blocking(session->flags)) {
                pr_info("The selected operation is of blocking type.\n");
                atomic_inc_thread_in_wait(session->priority, minor);
                pr_info("Increased number of threads in wait.\n");
                
                pr_info("Thread goes in wait...\n");
                res = custom_wait(buffer->waitqueue, lock_and_awake(byte_to_read(session->priority,minor) > 0, 
                                        &(buffer->op_mutex)), session->timeout*CONFIG_HZ);
                
                atomic_dec_thread_in_wait(session->priority, minor);
                pr_info("Decreased number of threads in wait.\n");

                // check result of wait
                if (res == 0) {
                        kfree(tmp_buf);
                        return 0;
                }
                if (res == -ERESTARTSYS) {
                        kfree(tmp_buf);
                        return -EINTR;
                }
        } 
        else {
                pr_info("The selected operation is of non-blocking type.\n");
                if (!mutex_trylock(&(buffer->op_mutex))) {
                        pr_info("Operation aborted: Token already in use by another thread.\n");
                        kfree(tmp_buf);
                        return -EBUSY;
                }
                if (is_empty(session->priority,minor)) {
                        pr_info("Operation aborted: Token acquired but the buffer is empty, no data to be read.\n");
                        mutex_unlock(&(buffer->op_mutex));
                        wake_up_interruptible(&(buffer->waitqueue));
                        kfree(tmp_buf);
                        return 0;
                }
        }
 
        pr_info("Start effective read.\n");

        if(len > byte_to_read(session->priority,minor)) len = byte_to_read(session->priority,minor);

        read_device_buffer(buffer, tmp_buf, len);
        sub_to_buffer(session->priority,minor,len);
        wake_up_interruptible(&(buffer->waitqueue));
        mutex_unlock(&(buffer->op_mutex));
        
        // copy from kernel to user space returns # of bytes that could not be copied  
        res = copy_to_user(buff,tmp_buf,len);
        pr_info("Bytes readed from the device: %d", len - res);
        pr_info("String readed from the device: %s", tmp_buf[len-res]);
        pr_info("Operation completed.\n");
        return len - res;
}

/**
 * device_write - write operation for a minor
 * @filp:       I/O session to the device file
 * @buff:       buffer that contain data to write
 * @len:        number of bytes to be written
 * @off:        offset to write to [not useful]
 * 
 * Returns:
 *  - # of written bytes when the operation is successful
 *  - a negative value when error occurs
 */
static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t *off) {
        int res;
        int byte_not_copied;
        int minor;
        char *tmp_buf;
        object_t *obj;
        session_t *session;
        device_manager_t *buffer;
        data_segment_t *to_write;

        // retrieve the obj related to the minor and the buffer related to the priority of the session of the thread
        minor = get_minor(filp);
        obj = devices + minor;
        session = (session_t *)filp->private_data;
        buffer = obj->buffer[session->priority];

        pr_info("Write operation called for minor: %d\n", minor);

        // copy data to write in a temporary buffer
        tmp_buf = kmalloc(len, session->flags);

        // copy from user to kernel space returns # of bytes that could not be copied  
        byte_not_copied = copy_from_user(tmp_buf, buff, len);

        // prepare memory areas
        to_write = kmalloc(sizeof(data_segment_t), session->flags);

        // check if thread must block
        if(is_blocking(session->flags)) {
                pr_info("The selected operation is of blocking type.\n");
                atomic_inc_thread_in_wait(session->priority, minor);
                pr_info("Increased number of threads in wait (+1).\n");
                
                pr_info("Thread goes in wait...\n");
                res = custom_wait(buffer->waitqueue, lock_and_awake(is_free(session->priority,minor),
                                &(buffer->op_mutex)), session->timeout*CONFIG_HZ);
                
                atomic_dec_thread_in_wait(session->priority, minor);
                pr_info("Decreased number of threads in wait (-1).\n");

                // check result of wait
                if (res == 0) goto free_area;
                if (res == -ERESTARTSYS) { res = -EINTR; goto free_area; }
        } 
        else {
                // check if token is available
                pr_info("The selected operation is of non-blocking type.\n");
                if (!mutex_trylock(&(buffer->op_mutex))) {
                        pr_info("Operation aborted: Token already in use by another thread.\n");
                        res = -EBUSY;
                        goto free_area;
                }
                else {
                        // check if data to write are available
                        if (is_empty(session->priority,minor)) {
                                pr_info("Operation aborted: Token acquired but nothing to write.\n");
                                res = 0;
                                goto unlock_wake;
                        }
                }

        }

        if (len-byte_not_copied > free_space(session->priority,minor)) len = free_space(session->priority,minor);
        else len = len - byte_not_copied;

        init_data_segment(to_write, tmp_buf, len);
        pr_info("Start effective write.\n");

        // write data segment
        if (session->priority == HIGH_PRIORITY) {
                pr_info("The selected operation is required at high priority.\n");
                write_device_buffer(buffer, to_write);
                add_to_buffer(HIGH_PRIORITY,minor,len);
                wake_up_interruptible(&(buffer->waitqueue));
        } 
        else {
                // asynchronous execution required to write on the low priority flow
                // da implementare
                pr_info("The selected operation is required at low priority.\n");
        }
        // release the token
        mutex_unlock(&(obj->buffer[session->priority]->op_mutex));
        pr_info("Bytes writed to the device: %s", buff);
        pr_info("Operation completed.\n");
        return len;

        // label for manage free and unlock in case of error
unlock_wake:    mutex_unlock(&(buffer->op_mutex));
                wake_up_interruptible(&(buffer->waitqueue));

free_area:      kfree(tmp_buf);
                free_data_segment(to_write);
                return res;
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
        case BLOCKING:
                session->flags = GFP_KERNEL;
                pr_info("Switched to blocking operations for minor: %d\n", minor);
                break;
        case UNBLOCKING:
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
        int i;
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

        // setup of structures
        for (i = 0; i < MINOR_NUMBER; i++) {
                // a workqueue and two buffers for each device
                devices[i].workqueue = create_singlethread_workqueue("work-queue-" + i);
                devices[i].buffer[LOW_PRIORITY] = kmalloc(sizeof(device_manager_t), GFP_KERNEL);
                devices[i].buffer[HIGH_PRIORITY] = kmalloc(sizeof(device_manager_t), GFP_KERNEL);
                init_device_manager(devices[i].buffer[LOW_PRIORITY]);
                init_device_manager(devices[i].buffer[HIGH_PRIORITY]);
        }

        // check errors in previous allocations
        if (i < MINOR_NUMBER) {
                for (; i > -1; i--) {
                        destroy_workqueue(devices[i].workqueue);
                        free_device_buffer(devices[i].buffer[LOW_PRIORITY]);
                        free_device_buffer(devices[i].buffer[HIGH_PRIORITY]);
                }
                return -ENOMEM;
        }

        printk(KERN_INFO "Kernel Module Inserted Successfully...\n");
        printk(KERN_INFO "%s: new driver registered, it is assigned major number %d\n",MODNAME, major);
        return 0;
}


/**
 * Module cleanup function
 */
void cleanup_module(void) {
        int i;
        __unregister_chrdev(major, 0, MINOR_NUMBER, DEVICE_NAME);
        printk(KERN_INFO "%s: driver with major number %d unregistered\n",MODNAME, major);
        
        // deallocation of structures
        for (i = 0; i < MINOR_NUMBER; i++) {
                destroy_workqueue(devices[i].workqueue);
                free_device_buffer(devices[i].buffer[LOW_PRIORITY]);
                free_device_buffer(devices[i].buffer[HIGH_PRIORITY]);
        }
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jacopo Fabi <jacopo.fabi1997@gmail.com");
MODULE_DESCRIPTION("Linux device driver that implements low and high priority flows of data");
MODULE_VERSION("1:1.0");
