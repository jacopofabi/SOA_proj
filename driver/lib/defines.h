#ifndef DEFINES_H
#define DEFINES_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/tty.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/errno.h>

/* GENERAL INFORMATION */
#define MODNAME "MULTIFLOW DRIVER"
#define DEVICE_NAME "multi-flow device"
#define MINOR_NUMBER 128
#define FLOWS 2         // number of different priority
#define LOW_PRIORITY 0  // index of low priority
#define HIGH_PRIORITY 1 // index of high priority

/* IOCTL VALUES */
#define TO_HIGH_PRIORITY 3
#define TO_LOW_PRIORITY 4
#define BLOCKING 5
#define UNBLOCKING 6
#define TIMEOUT 7

/* BOUNDS */
#define MIN_SECONDS 1                // minimum amount of seconds
#define MAX_SECONDS 17179869         // maximum amount of seconds
#define MAX_BYTE_IN_BUFFER 32 * 4096 // maximum number of byte in buffer
#define CONFIG_HZ 250

/* STRUCTURES DEFINITION */

/** 
 * Session informations (unique for each thread)
 * session_t - I/O session
 * @priority:   priority of session
 * @flags:      type of session, blocking or not
 * @timeout:    timeout for blocking operations
 */
typedef struct session {
        short priority;
        gfp_t flags;
        unsigned long timeout;
} session_t;

/** 
 * Object that handles mutex, waitqueue and linked list for r/w operations on a priority flow of a specific minor
 * device_manager_t - Device manager
 * @head:       head of linked list
 * @op_mutex:   mutex to synchronize operations in buffer
 * @waitqueue:  waitqueue for the specific minor
 */
typedef struct device_manager {
        struct list_head head;
        struct mutex op_mutex;
        wait_queue_head_t waitqueue;
} device_manager_t;

/** 
 * Single buffer associated to a specific linked list related to a priority flow of a specific minor
 * data_segment_t - data segment
 * @list:       list_head element to link to list
 * @content:    byte content of data segment
 * @byte_read:  number of byte read up to instant t
 * @size:       size of data segment content
 */
typedef struct data_segment {
        struct list_head list;
        char *content;
        int byte_read;
        int size;
} data_segment_t;

/** 
 * Object that handles device manager for the two priority flows and a workqueue for a specific minor
 * object_t - I/O object
 * @workqueue:  pointer to workqueue for low priority flow
 * @buffer:     device manager for low and high priority
 */
typedef struct object {
        struct workqueue_struct *workqueue;
        device_manager_t *buffer[FLOWS];
} object_t;


/* DEVICE MANAGER FUNCTION PROTOTYPES */
void init_device_manager(device_manager_t *);
void init_data_segment(data_segment_t *, char *, int);
void free_device_buffer(device_manager_t *);
void free_data_segment(data_segment_t *);
void write_device_buffer(device_manager_t *, data_segment_t *);
void read_device_buffer(device_manager_t *, char *, int);


/* MACROS DEFINITION */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session) MAJOR(session->f_inode->i_rdev)
#define get_minor(session) MINOR(session->f_inode->i_rdev)
#else
#define get_major(session) MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session) MINOR(session->f_dentry->d_inode->i_rdev)
#endif

// we avoid check on priority with a simple operation using a single vector for byte_in_buffer and thread_in_wait parameters
// priority low = 0 --> refers to buffers/threads from 0 to 128 (buffers/threads with low priority)
// priority high = 1 --> refers to buffers/threads from 129 to 256 (buffers/threads with high priority)
#define get_buffer_index(priority, minor) ((priority * MINOR_NUMBER) + minor)
#define get_thread_index(priority, minor) ((priority * MINOR_NUMBER) + minor)
#define byte_to_read(priority, minor) byte_in_buffer[get_buffer_index(priority, minor)]

#define get_seconds(sec) (sec > MAX_SECONDS ? sec = MAX_SECONDS : (sec == 0 ? sec = MIN_SECONDS : sec))
#define used_space(priority, minor) (priority == LOW_PRIORITY ? (byte_in_buffer[get_buffer_index(priority, minor)] + booked_byte[minor]) : byte_in_buffer[get_buffer_index(priority, minor)])
#define free_space(priority, minor) MAX_BYTE_IN_BUFFER - used_space(priority, minor)
#define is_free(priority, minor) (free_space(priority, minor) == 0 ? 0 : 1)
#define is_empty(priority, minor) (byte_to_read(priority, minor) == 0 ? 1 : 0)
#define is_blocking(flags) (flags == GFP_ATOMIC ? 0 : 1)
#define add_to_buffer(priority, minor, len) byte_in_buffer[get_buffer_index(priority, minor)] += len
#define add_booked_byte(minor, len) booked_byte[minor] += len
#define sub_to_buffer(priority, minor, len) byte_in_buffer[get_buffer_index(priority, minor)] -= len
#define sub_booked_byte(minor, len) booked_byte[minor] -= len
#define atomic_inc_thread_in_wait(priority, minor) __sync_fetch_and_add(thread_in_wait + get_thread_index(priority, minor), 1)
#define atomic_dec_thread_in_wait(priority, minor) __sync_fetch_and_sub(thread_in_wait + get_thread_index(priority, minor), 1)


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#define custom_wait wait_event_interruptible_exclusive_timeout
#else
/**
 * The process goes to sleep until the condition gets true or a timeout elapses.
 * The condition is checked each time the waitqueue is woken up.
 * 
 * wait_event_interruptible_timeout
 * @wq:         waitqueue to wait on
 * @condition:  expression for the event to wait for
 * @timeout:    timeout, in jiffies
 * 
 * Returns:
 * - 0 if the @condition evaluated to false after the @timeout elapsed,
 * - 1 if the @condition evaluated to true after the @timeout elapsed,
 * - the remaining jiffies if the @condition evaluated to true before the @timeout elapsed, 
 * - ERESTARTSYS if it was interrupted by a signal.
 */
#define custom_wait wait_event_interruptible_timeout
#endif


/**
 * This macro allow to put in waitqueue a task in exclusive mode and set a timeout.
 * The call to ___wait_event() is the same used in __wait_event_interruptible_timeout() 
 * in the macro wait_event_interruptible_timeout(), but with exclusive mode enabled.   
 * 
 * __wait_event_interruptible_exclusive_timeout
 * @wq_head:    the waitqueue to wait on
 * @condition:  expression for the event to wait for
 * @timeout:    timeout, in jiffies
 */
#define __wait_event_interruptible_exclusive_timeout(wq_head, condition, timeout)               \
        ___wait_event(wq_head, ___wait_cond_timeout(condition), TASK_INTERRUPTIBLE, 1, timeout, \
                      __ret = schedule_timeout(__ret))


/**
 * This macro is equal to wait_event_interruptible_timeout with exclusive mode is enabled.
 * We change __wait_event_interruptible_exclusive_timeout instead of __wait_event_interruptible_timeout.
 * 
 * The process goes to sleep until the @condition evaluates to true or a signal is received.
 * The @condition is checked each time the waitqueue @wq_head is woken up.
 *
 * wait_event_interruptible_exclusive_timeout
 * @wq_head:    head of waitqueue
 * @condition:  awakeness condition
 * @timeout:    awakeness timeout
 *
 * Returns:
 * - 0 if the @condition evaluated to false after the @timeout elapsed,
 * - 1 if the @condition evaluated to true after the @timeout elapsed,
 * - the remaining jiffies if the @condition evaluated to true before the @timeout elapsed, 
 * - ERESTARTSYS if it was interrupted by a signal.
 */
#define wait_event_interruptible_exclusive_timeout(wq_head, condition, timeout) ({	                \
	long __ret = timeout;							                        \
	might_sleep();								                        \
	if (!___wait_cond_timeout(condition)) __ret = __wait_event_interruptible_exclusive_timeout      \
                                                        (wq_head, condition, timeout);		        \
	__ret; })


/**
 * This macro returns a value of condition that is evaluated in the macro 
 * wait_event_interruptible_exclusive_timeout.
 *
 * lock_and_awake
 * @condition:  condition to evaluate
 * @mutex:      pointer to mutex to try lock
 */
#define lock_and_awake(condition, mutex) ({         \
        int __ret = 0;                              \
        if (mutex_trylock(mutex)) {                 \
                if (condition) __ret = 1;           \
                else mutex_unlock(mutex);           \
        }                                           \
        __ret; })
#endif