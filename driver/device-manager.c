#include "lib/defines.h"

/**
 * init_device_manager - initialization of the device: buffer, mutex and workqueue
 * @device:     pointer to the device to initialize
 */
void init_device_manager(device_manager_t *device) {
        struct list_head *head;
        head = &(device->head);
        mutex_init(&(device->op_mutex));
        init_waitqueue_head(&(device->waitqueue));
        INIT_LIST_HEAD(head);
}

/**
 * init_data_segment - initialization of data segment
 * @element:    pointer to data segment to initialize
 * @content:    content of data segment
 * @len:        size of data segment content
 */
void init_data_segment(data_segment_t *element, char *content, int len) {
        element->content = content;
        element->size = len;
        element->byte_read = 0;
}

/**
 * write_device_buffer - put in list a new data segment
 * @device:     pointer to device that handles the buffer to edit
 * @segment:    pointer to data segment to add
 */
void write_device_buffer(device_manager_t *device, data_segment_t *segment) {
        list_add_tail(&(segment->list), &(device->head));
}

/**
 * read_device_buffer - read data in buffer
 * @device:             pointer device that handles the buffer to read
 * @read_content:       buffer that contains read data
 * @len:                bytes number to be read
 */
void read_device_buffer(device_manager_t *device, char *read_content, int len) {
        struct list_head *cur;
        struct list_head *old;
        struct list_head *head;
        data_segment_t *cur_seg;
        int byte_read;

        head = &(device->head);
        cur = (head)->next;
        cur_seg = list_entry(cur, struct data_segment, list);
        byte_read = 0;

        while (len - byte_read >= cur_seg->size - cur_seg->byte_read) {
                memcpy(read_content + byte_read, cur_seg->content + cur_seg->byte_read, cur_seg->size - cur_seg->byte_read);
                byte_read += cur_seg->size - cur_seg->byte_read;
                old = cur;
                cur = cur->next;
                free_data_segment(cur_seg);
                list_del(old);
                
                if (cur == head) break;

                cur_seg = list_entry(cur, struct data_segment, list);
        }
        
        // check if i must read in this condition: byte_to_read < cur->size
        if (cur != head) {
                cur_seg = list_entry(cur, struct data_segment, list);
                memcpy(read_content + byte_read, cur_seg->content + cur_seg->byte_read, len - byte_read);
                cur_seg->byte_read += len - byte_read;
                byte_read += len - byte_read;
        }
}

/**
 * free_data_segment - free a data segment
 * @segment:    pointer to data segment to free
 */
void free_data_segment(data_segment_t *segment) {
        kfree(segment->content);
        kfree(segment);
        return;
}

/**
 * free_device_buffer - free a buffer
 * @device:     pointer to device that handle the buffer to free
 */
void free_device_buffer(device_manager_t *device) { 
        struct list_head *cur;
        struct list_head *old;
        struct list_head *head;
        data_segment_t *cur_seg;

        head = &(device->head);
        old = NULL;

        list_for_each(cur, head) {
                if (old) list_del(old);
                cur_seg = list_entry(cur, data_segment_t, list);
                free_data_segment(cur_seg);
                old = cur;
        }

        mutex_destroy(&(device->op_mutex));
        kfree(device);
        return;
}