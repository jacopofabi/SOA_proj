#ifndef DEFINES_H
#define DEFINES_H

#include <stdbool.h>
#include <unistd.h>

#define MAX_BUF_SIZE 50

/* switch values */
#define TO_HIGH_PRIORITY        1
#define TO_LOW_PRIORITY         2
#define BLOCKING                3
#define UNBLOCKING              4
#define TIMEOUT                 5
#define ENABLE                  6
#define DISABLE                 7
#define WRITE                   8
#define READ                    9
#define RELEASE                 10

/* Signal for async notification */
#define SIGETX    44

/** ioctl commands re-definition
*   The ioctl function requires the same values used by the driver: 3,4,5,6,7 as defined in /driver/lib/defines.h 
*/

#define set_high_priority(fd)           ioctl(fd, 3)
#define set_low_priority(fd)            ioctl(fd, 4)
#define set_blocking_operations(fd)     ioctl(fd, 5)
#define set_unblocking_operations(fd)   ioctl(fd, 6)
#define set_timeout(fd, value)          ioctl(fd, 7, value)
#define enable_device(fd)               ioctl(fd, 8)
#define disable_device(fd)              ioctl(fd, 9)

/* driver operations */
#define device_open(path, flags)        open(path, flags)
#define device_release(fd)              close(fd)
#define device_read(fd, buff, size)     read(fd, buff, size)
#define device_write(fd, buff, size)    write(fd, buff, size)

#endif
