#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <stdbool.h>

#define MAX_BUF_SIZE 50

/* ioctl commands re-definition */
#define set_high_priority(fd)       ioctl(fd, 3)
#define set_low_priority(fd)        ioctl(fd, 4)
#define set_blocking_operations(fd)     ioctl(fd, 5)
#define set_unblocking_operations(fd)   ioctl(fd, 6)
#define set_timeout(fd, value)          ioctl(fd, 7, value)

#endif
