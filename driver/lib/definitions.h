#ifndef DEFINITIONS_H
#define DEFINITIONS_H

/* GENERAL INFORMATION */
#define MODNAME "MULTIFLOW DRIVER"
#define MINOR_NUMBER 128
#define DEVICE_NAME "multi-flow device"
#define FLOWS 2                                         // number of different priority
#define LOW_PRIORITY 0                                  // index of low priority
#define HIGH_PRIORITY 1                                 // index of high priority


// we avoid check on priority with a simple mathematic operation using a single vector for byte_in_buffer module parameter
// priority low = 0 --> get bytes of buffers from 0 to 128 (buffers with low priority)
// priority high = 1 --> get bytes of buffers from 129 to 256 (buffers with high priority)
#define get_byte_in_buffer_index(priority, minor)   ((priority * MINOR_NUMBER) + minor)

#endif
