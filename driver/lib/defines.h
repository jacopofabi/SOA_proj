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
#include <linux/signal.h>
#include <linux/jiffies.h>

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
#define ENABLE 8
#define DISABLE 9

/* BOUNDS */
#define MIN_SECONDS 1                                    // minimum amount of seconds for timeout
#define MAX_SECONDS 3600                                 // maximum amount of seconds for timeout
#define MAX_BYTE_IN_BUFFER 32 * 4096                     // maximum number of byte in buffer: 5096 * sizeof(data_segment_t)

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
 * Object that handles mutex, waitqueue and buffer of data segments related to a priority flow of a specific minor
 * flow_manager_t - Manager of a priority flow
 * @head:       head of linked list
 * @op_mutex:   mutex to synchronize operations in buffer
 * @waitqueue:  waitqueue for the specific minor
 */
typedef struct flow_manager {
        struct list_head head;
        struct mutex op_mutex;
        wait_queue_head_t waitqueue;
} flow_manager_t;

/** 
 * Single data segment of a linked list that represents a buffer, related to a priority flow of a specific minor
 * data_segment_t - data segment
 * @entry:      list_head element to link to list
 * @content:    byte content of data segment
 * @byte_read:  number of byte read up to instant t
 * @size:       size of data segment content
 */
typedef struct data_segment {
        struct list_head entry;
        char *content;
        int byte_read;
        int size;
} data_segment_t;

/** 
 * Object that handles device manager for the two priority flows and a workqueue for a specific minor
 * device_manager_t - Manager of a device file
 * @workqueue:  pointer to workqueue for low priority flow
 * @buffer:     device manager for low and high priority
 */
typedef struct device_manager {
        struct workqueue_struct *workqueue;
        flow_manager_t *flow[FLOWS];
} device_manager_t;

/**
 * async_task_t - deffered work
 * @del_work:           delayed_work struct uses a timer to run after the specified time interval
 * @to_write:           pointer to data segment to write
 * @session:            session to the device file
 * @minor:              minor number of the device
 */
typedef struct async_task {
        struct delayed_work del_work;
        data_segment_t *to_write;
        session_t *session;
        int minor;
} async_task_t;


/* FLOW MANAGER FUNCTION PROTOTYPES */
void init_flow_manager(flow_manager_t *);
void init_data_segment(data_segment_t *, char *, int);
void write_data_segment(flow_manager_t *, data_segment_t *);
void read_from_flow(flow_manager_t *, char *, int);
void free_data_segment(data_segment_t *);
void free_flow(flow_manager_t *);


/* MACROS DEFINITION */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session) MAJOR(session->f_inode->i_rdev)
#define get_minor(session) MINOR(session->f_inode->i_rdev)
#else
#define get_major(session) MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session) MINOR(session->f_dentry->d_inode->i_rdev)
#endif

// we avoid check on priority with a simple operation using a single vector for bytes_in_buffer and threads_in_wait parameters
// priority low = 0 --> refers to buffers/threads from 0 to 127 (buffers/threads with low priority)
// priority high = 1 --> refers to buffers/threads from 128 to 255 (buffers/threads with high priority)
#define get_buffer_index(priority, minor) ((priority * MINOR_NUMBER) + minor)
#define get_thread_index(priority, minor) ((priority * MINOR_NUMBER) + minor)
#define byte_to_read(priority, minor) bytes_in_buffer[get_buffer_index(priority, minor)]

#define get_seconds(sec) (sec > MAX_SECONDS ? sec = MAX_SECONDS : (sec == 0 ? sec = MIN_SECONDS : sec))
#define used_space(priority, minor) (priority == LOW_PRIORITY ? (bytes_in_buffer[get_buffer_index(priority, minor)]) : bytes_in_buffer[get_buffer_index(priority, minor)])
#define free_space(priority, minor) MAX_BYTE_IN_BUFFER - used_space(priority, minor)
#define is_free(priority, minor) (free_space(priority, minor) > 0 ? 1 : 0)
#define is_empty(priority, minor) (byte_to_read(priority, minor) == 0 ? 1 : 0)
#define is_blocking(flags) (flags == GFP_KERNEL ? 1 : 0)
#define add_to_buffer(priority, minor, len) bytes_in_buffer[get_buffer_index(priority, minor)] += len
#define sub_to_buffer(priority, minor, len) bytes_in_buffer[get_buffer_index(priority, minor)] -= len
#define inc_thread_in_wait(priority, minor) __sync_fetch_and_add(threads_in_wait + get_thread_index(priority, minor), 1)
#define dec_thread_in_wait(priority, minor) __sync_fetch_and_sub(threads_in_wait + get_thread_index(priority, minor), 1)

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
 * This macro is equal to existing wait_event_interruptible_timeout with exclusive mode enabled.
 * We change __wait_event_interruptible_exclusive_timeout instead of __wait_event_interruptible_timeout.
 * 
 * The process goes to sleep until the @condition evaluates to true or a timeout elapses.
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
 * 
 * The process try to acquire the lock, if it is possible returns:
 * - 1 if the @condition evaluated to true,
 * - 0 if the @condition evaluated to false, with the release of the lock.
 */
#define lock_and_awake(condition, mutex) ({         \
        int __ret = 0;                              \
        if (mutex_trylock(mutex)) {                 \
                if (condition) __ret = 1;           \
                else mutex_unlock(mutex);           \
        }                                           \
        __ret; })
#endif