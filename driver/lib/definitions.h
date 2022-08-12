#ifndef DEFINITIONS_H
#define DEFINITIONS_H

/* GENERAL INFORMATION */
#define MODNAME "MULTIFLOW DRIVER"
#define MINOR_NUMBER 128
#define DEVICE_NAME "multi-flow device"
#define FLOWS 2                                         // number of different priority
#define LOW_PRIORITY 0                                  // index of low priority
#define HIGH_PRIORITY 1                                 // index of high priority

/* ioctl values */
#define TO_HIGH_PRIORITY        3                       
#define TO_LOW_PRIORITY         4
#define BLOCK                   5
#define UNBLOCK                 6
#define TIMEOUT                 7

/* bounds for timeout */
#define MIN_SECONDS             1                       // minimum amount of seconds
#define MAX_SECONDS             17179869                // maximum amount of seconds


/*STRUCTURES DEFINITION*/

/* Session informations
 * session_t - I/O session
 * @priority:   priority of session
 * @flags:      flags used for allocation (blocking or not)
 * @timeout:    timeout for blocking operations
 */
typedef struct session {
        short priority;
        gfp_t flags;
        unsigned long timeout;
} session_t;


/* MACRO DEFINITION */
#define get_seconds(sec)        (sec > MAX_SECONDS ? sec = MAX_SECONDS : (sec == 0 ? sec = MIN_SECONDS : sec))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)      MAJOR(session->f_inode->i_rdev)
#define get_minor(session)      MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)      MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)      MINOR(session->f_dentry->d_inode->i_rdev)
#endif

// we avoid check on priority with a simple mathematic operation using a single vector for byte_in_buffer module parameter
// priority low = 0 --> get bytes of buffers from 0 to 128 (buffers with low priority)
// priority high = 1 --> get bytes of buffers from 129 to 256 (buffers with high priority)
#define get_byte_in_buffer_index(priority, minor)   ((priority * MINOR_NUMBER) + minor)

#endif
