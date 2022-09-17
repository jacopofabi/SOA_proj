/********************************************************************************
*  \file       multi-flow-device.c
*
*  \author     Jacopo Fabi
*
*  \details    Linux multiflow device driver implementation
*
* *******************************************************************************/
#include "lib/defines.h"

/* Module parameters */
bool enabled[MINOR_NUMBER] = {[0 ... (MINOR_NUMBER-1)] = true};                         //state of the device files
long bytes_in_buffer[FLOWS * MINOR_NUMBER];                                              //#bytes in manager for every minor
long threads_in_wait[FLOWS * MINOR_NUMBER];                                              //#threads in wait on manager for every minor
module_param_array(enabled, bool, NULL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(enabled_device, "Module parameter implemented in order to enable or disable a device with a specific \
minor number. If it is disabled, any attempt to open a session should fail, except for already opened sessions.");
module_param_array(bytes_in_buffer, long, NULL, S_IRUSR | S_IRGRP);
MODULE_PARM_DESC(hp_bytes, "Number of bytes currently present in low and high priority flows.");
module_param_array(threads_in_wait, long, NULL, S_IRUSR | S_IRGRP);
MODULE_PARM_DESC(hp_threads, "Number of threads currently in wait on low and high priority flows.");

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
int setup_operation(device_manager_t *, session_t *, int, char *);
void async_write(struct delayed_work *);

/* Signaling for asynchronous notification */
struct kernel_siginfo info;
static int signum = 0;

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
        .unlocked_ioctl = device_ioctl,
        .read = device_read,
        .write = device_write
};


/**
 * device_open - session opening for a minor
 * @inode:      I/O metadata of the device file
 * @file:       I/O session to the device file
 */
static int device_open(struct inode *inode, struct file *filp) {
        session_t *session;
        int minor = get_minor(filp);
        if (minor >= MINOR_NUMBER) { return -ENODEV; }
        pr_info("Session opened for minor: %d\n", minor);
        session = kmalloc(sizeof(session_t), GFP_KERNEL);
        session->priority = HIGH_PRIORITY;
        session->flags = GFP_KERNEL;
        session->timeout = MAX_SECONDS;
        filp->private_data = session;
        return 0;
}

/**
 * device_release - close session for a minor
 * @inode:      I/O metadata of the device file
 * @file:       I/O session to the device file
 */
static int device_release(struct inode *inode, struct file *filp) {
        int minor = get_minor(filp);
        pr_info("Session closed for minor: %d\n", minor);
        kfree(filp->private_data);
        filp->private_data = NULL;
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
        case BLOCKING:
                session->flags = GFP_KERNEL;
                pr_info("Switched to blocking operations for minor: %d\n", minor);
                break;
        case UNBLOCKING:
                session->flags = GFP_ATOMIC;
                pr_info("Switched to unblocking operations for minor: %d\n", minor);
                break;
        case TIMEOUT:
                session->flags = GFP_KERNEL;
                session->timeout = get_seconds(param);
                pr_info("Setup of timeout for blocking operations to %ld sec for minor: %d\n", session->timeout, minor);
                break;
        default:
                return -ENOTTY;
        }
        return 0;
}

/**
 * device_read - read operation for a minor
 * @filp:       I/O session to the device file
 * @buff:       pointer to a manager that contains read data
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
        char *final;
        ssize_t valid;
        object_t *obj;
        session_t *session;
        device_manager_t *manager;

        // retrieve the obj related to the minor and the manager related to the priority of the session of the thread
        minor = get_minor(filp);
        obj = devices + minor;
        session = (session_t *)filp->private_data;
        manager = obj->manager[session->priority];

        pr_info("Read operation called for minor: %d\n", minor);
        if (len <= 0) return 0;

        tmp_buf = kmalloc(len, session->flags);

        // setup for blocking or non-blocking operation
        res = setup_operation(manager, session, minor, "read");
        if (!res) { goto free_area; } //else we have the lock
        
        pr_info("Start effective read.\n");

        if(len > byte_to_read(session->priority,minor)) len = byte_to_read(session->priority,minor);
        read_device_buffer(manager, tmp_buf, len);
        sub_to_buffer(session->priority,minor,len);
        wake_up_interruptible(&(manager->waitqueue));
        mutex_unlock(&(manager->op_mutex));
        
        // copy data to read on a buffer, from kernel to user space returns # of bytes that could not be copied  
        res = copy_to_user(buff,tmp_buf,len);
        valid = len - res;
        final = kmalloc(valid, session->flags);
        memcpy(final, tmp_buf, valid);

        pr_info("Operation completed, bytes readed from the device: %zu, %s\n", valid, final);
        return len - res;

        // label for manage memory release in case of error
free_area:      kfree(tmp_buf);
                return res;
}

/**
 * device_write - write operation for a minor
 * @filp:       I/O session to the device file
 * @buff:       manager that contain data to write
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
        device_manager_t *manager;
        data_segment_t *to_write;
        async_task_t *task;

        // retrieve the obj related to the minor and the manager related to the priority of the session
        minor = get_minor(filp);
        obj = devices + minor;
        session = (session_t *)filp->private_data;
        manager = obj->manager[session->priority];
        task = NULL;

        pr_info("Write operation called for minor: %d\n", minor);
        if (len <= 0) return 0;

        // copy data to write on a temp buffer, from user to kernel space returns # of bytes that colud not be copied
        tmp_buf = kmalloc(len, session->flags);
        byte_not_copied = copy_from_user(tmp_buf, buff, len);

        // prepare memory areas
        to_write = kmalloc(sizeof(data_segment_t), session->flags);

        // setup for blocking or non-blocking operation
        res = setup_operation(manager, session, minor, "write");
        if (!res) { goto free_area; } //else we have the lock
        
        pr_info("Start effective write.\n");
        
        if (len - byte_not_copied > free_space(session->priority,minor)) len = free_space(session->priority,minor);
        else len = len - byte_not_copied;
        init_data_segment(to_write, tmp_buf, len);

        // check if data segment must be write in a synchronous way
        if (session->priority == HIGH_PRIORITY) {
                pr_info("The selected operation is required at high priority.\n");
                write_device_buffer(manager, to_write);
                add_to_buffer(HIGH_PRIORITY, minor, len);
                wake_up_interruptible(&(manager->waitqueue));
                mutex_unlock(&(obj->manager[session->priority]->op_mutex));
                pr_info("Operation completed, bytes writed to the device at high priority: %s\n", tmp_buf);
        } 
        else {
                pr_info("The selected operation is required at low priority.\n");
                if(!try_module_get(THIS_MODULE)) {
                        wake_up_interruptible(&(manager->waitqueue));
                        mutex_unlock(&(manager->op_mutex));
                        res = -ENODEV;
                        goto free_area;
                }
                // setup the asynchronous task
                task = kmalloc(sizeof(async_task_t), session->flags);
                task->to_write = to_write;
                task->minor = minor;
                
                // save PID for signalling
                task->proc = get_current(); 
                signum = SIGETX;       

                // initialize an already declared delayed_work item with a deffered write handler
                pr_info("Insert thread in the workqueue...\n");
                INIT_DELAYED_WORK(&(task->del_work), (void *)async_write);
                add_booked_byte(minor,len);
                // insert the async task in the workqueue associated to the device after a delay of 5sec
                queue_delayed_work(obj->workqueue, &(task->del_work), msecs_to_jiffies(5000));
        }
        return len;

        // label for manage memory release in case of error
free_area:      kfree(tmp_buf);
                free_data_segment(to_write);
                kfree(task);
                return res;
}

/**
 * setup_operation - try to setup and initialize a blocking or non-blocking read/write for a specific minor
 * @manager:    object that handles data structures for a specific minor
 * @session:    I/O session to the device file
 * @minor:      minor number of the device file
 * @type:       type of operation, read or write
 * 
 * Returns:
 *  - 1 if the operation is completed successfully (lock acquired and condition checked for read/write),
 *  - 0 or a specific error otherwise.
 */
int setup_operation(device_manager_t *manager, session_t *session, int minor, char *type) {
        int res;
        if (strcmp(type, "read") != 0 && strcmp(type, "write") != 0) { return 0; }

        // check if thread must block
        if(is_blocking(session->flags)) {
                pr_info("The selected operation is of blocking type.\n");
                inc_thread_in_wait(session->priority, minor);
                pr_info("Increased number of threads in wait (+1).\n");
                
                pr_info("Thread goes in wait...\n");
                // BLOCKING READ: wait until there are bytes to read to take the lock
                if (strcmp(type, "read") == 0) { 
                        res = custom_wait(manager->waitqueue, lock_and_awake(
                                byte_to_read(session->priority,minor) > 0, &(manager->op_mutex)), msecs_to_jiffies(session->timeout*1000)); 
                }
                // BLOCKING WRITE: wait until there is space to write to take the lock
                if (strcmp(type, "write") == 0) { 
                        res = custom_wait(manager->waitqueue, lock_and_awake(
                                is_free(session->priority,minor), &(manager->op_mutex)), msecs_to_jiffies(session->timeout*1000)); 
                }
                dec_thread_in_wait(session->priority, minor);
                pr_info("Decreased number of threads in wait (-1).\n");

                // check if error on wait: token not available after timeout elapsed or signal interruption
                if (res == 0) { return res; }
                if (res == -ERESTARTSYS) { return -EINTR; }
        }
        else {
                // check if token is available
                pr_info("The selected operation is of non-blocking type.\n");
                if (!mutex_trylock(&(manager->op_mutex))) {
                        pr_info("Operation aborted: Token already in use by another thread.\n");
                        return -EBUSY;
                }
                // NON-BLOCKING READ
                if (strcmp(type, "read") == 0) {
                        // check if data to read are available
                        if (is_empty(session->priority,minor)) {
                                pr_info("Operation aborted: Token acquired but the buffer is empty, no data to be read.\n");
                                wake_up_interruptible(&(manager->waitqueue));
                                mutex_unlock(&(manager->op_mutex));
                                return 0;
                        }
                }
                // NON-BLOCKING WRITE
                if (strcmp(type, "write") == 0) {
                        // check if data can be writed
                        if (is_free(session->priority,minor)) {
                                pr_info("Operation aborted: Token acquired but the buffer is full, no data can be writed.\n");
                                wake_up_interruptible(&(manager->waitqueue));
                                mutex_unlock(&(manager->op_mutex));
                                return 0;
                        }
                }
        }
        // token successfully acquired will be released in the read/write functions
        return 1;
}

/**
 * async_write - asynchronous write for low priority flow
 * @data:      pointer to the delayed_work struct that runs the deffered write
 * 
 */
void async_write(struct delayed_work *data) {
        // we retrieve the async_task_t struct address using the member delayed_work address, data points to del_work
        async_task_t *task = container_of((void*)data, async_task_t, del_work);
        object_t *object = devices + task->minor;
        device_manager_t *manager = object->manager[LOW_PRIORITY];
        
        write_device_buffer(object->manager[LOW_PRIORITY], task->to_write);
        sub_booked_byte(task->minor,task->to_write->size);
        add_to_buffer(LOW_PRIORITY, task->minor, task->to_write->size);
        pr_info("Operation completed, bytes writed to the device at low priority: %s\n", task->to_write->content);
        
        // release token acquired in setup_operation
        wake_up_interruptible(&(object->manager[LOW_PRIORITY]->waitqueue));
        mutex_unlock(&(manager->op_mutex));
        kfree(container_of((void*)data, async_task_t, del_work));
        module_put(THIS_MODULE);

        // send notification to app
        memset(&info, 0, sizeof(struct kernel_siginfo));
        info.si_signo = SIGETX;
        info.si_code = SI_QUEUE;
        info.si_int = 1;

        pr_info("Sending signal to app...\n");
        if(send_sig_info(SIGETX, &info, task->proc) < 0) { pr_info("Unable to send signal\n"); }
        pr_info("Send!\n");
}


/**
 * Module initialization function
 */
int init_module(void) {
        int i;
        pr_info("Welcome!\n"); 
        pr_info("This is a multiflow device driver implementation of Jacopo Fabi\n");

        // dynamic allocation of major number specifying the base minor number and the count of minor devices
        // device driver --> /proc/devices ; device files --> /dev
        major = __register_chrdev(0, 0, MINOR_NUMBER, DEVICE_NAME, &fops);
        if (major < 0) {
                pr_info("%s: cannot allocate major number\n", MODNAME);
                return major;
        }
        // setup of structures
        for (i = 0; i < MINOR_NUMBER; i++) {
                // a workqueue and two managers, one for each priority flow of the specific device
                devices[i].workqueue = create_singlethread_workqueue("work-queue-" + i);
                devices[i].manager[LOW_PRIORITY] = kmalloc(sizeof(device_manager_t), GFP_KERNEL);
                devices[i].manager[HIGH_PRIORITY] = kmalloc(sizeof(device_manager_t), GFP_KERNEL);
                init_device_manager(devices[i].manager[LOW_PRIORITY]);
                init_device_manager(devices[i].manager[HIGH_PRIORITY]);
        }
        // check errors in previous allocations
        if (i < MINOR_NUMBER) {
                for (; i > -1; i--) {
                        destroy_workqueue(devices[i].workqueue);
                        free_device_buffer(devices[i].manager[LOW_PRIORITY]);
                        free_device_buffer(devices[i].manager[HIGH_PRIORITY]);
                }
                return -ENOMEM;
        }
        pr_info("Kernel Module Inserted Successfully...\n");
        pr_info("%s: new driver registered, it is assigned major number %d\n",MODNAME, major);
        return 0;
}

/**
 * Module cleanup function
 */
void cleanup_module(void) {
        int i;
        __unregister_chrdev(major, 0, MINOR_NUMBER, DEVICE_NAME);
        pr_info("%s: driver with major number %d unregistered\n",MODNAME, major);
        
        // deallocation of structures
        for (i = 0; i < MINOR_NUMBER; i++) {
                destroy_workqueue(devices[i].workqueue);
                free_device_buffer(devices[i].manager[LOW_PRIORITY]);
                free_device_buffer(devices[i].manager[HIGH_PRIORITY]);
        }
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jacopo Fabi <jacopo.fabi1997@gmail.com");
MODULE_DESCRIPTION("Linux device driver that implements low and high priority flows of data");
MODULE_VERSION("1:1.0");