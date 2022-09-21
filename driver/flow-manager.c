#include "lib/defines.h"

/**
 * init_flow_manager - initialization of the flow: linked list of data segments, mutex and waitqueue
 * @flow:     pointer to flow manager to initialize
 */
void init_flow_manager(flow_manager_t *flow) {
        struct list_head *head;
        head = &(flow->head);
        mutex_init(&(flow->op_mutex));
        init_waitqueue_head(&(flow->waitqueue));
        INIT_LIST_HEAD(head);
}

/**
 * init_data_segment - initialization of data segment
 * @element:    pointer to data segment to initialize
 * @content:    pointer to content of data segment
 * @len:        size of the content
 */
void init_data_segment(data_segment_t *element, char *content, int len) {
        element->content = content;
        element->size = len;
        element->byte_read = 0;
}

/**
 * write_data_segment - add new data segment to flow
 * @flow:       pointer to flow manager that handles the linked list to edit
 * @segment:    pointer to data segment to add before the specified head
 *
 * Add new data segment at the end of the list, so we build the flow as a FIFO queue.
 */
void write_data_segment(flow_manager_t *flow, data_segment_t *segment) {
        list_add_tail(&(segment->list), &(flow->head));
}

/**
 * read_from_flow - read data from flow
 * @flow:               pointer to flow manager that handles the linked list to read
 * @read_content:       buffer that is filled with read data
 * @len:                number of bytes to be read
 */
void read_from_flow(flow_manager_t *flow, char *read_content, int len) {
        struct list_head *head, *cur, *old;
        data_segment_t *cur_seg;
        int byte_read;

        head = &(flow->head);
        cur = head->next;
        cur_seg = list_entry(cur, data_segment_t, list);
        byte_read = 0;

        // read data from one or more data segments
        while (len - byte_read >= cur_seg->size - cur_seg->byte_read) {
                memcpy(read_content + byte_read, cur_seg->content + cur_seg->byte_read, cur_seg->size - cur_seg->byte_read);
                byte_read += cur_seg->size - cur_seg->byte_read;
                old = cur;
                cur = cur->next;
                free_data_segment(cur_seg);
                list_del(old);

                if (cur == head) break;

                cur_seg = list_entry(cur, data_segment_t, list);
        }

        // check if i must read in this condition: byte_to_read < cur->size
        if (cur != head) {
                cur_seg = list_entry(cur, data_segment_t, list);
                memcpy(read_content + byte_read, cur_seg->content + cur_seg->byte_read, len - byte_read);
                cur_seg->byte_read += len - byte_read;
                byte_read += len - byte_read;
        }
}

/**
 * free_data_segment - release memory of a data segment
 * @segment:    pointer to data segment to free
 */
void free_data_segment(data_segment_t *segment) {
        kfree(segment->content);
        kfree(segment);
        return;
}

/**
 * free_flow - release memory of the flow
 * @flow:     pointer to flow manager that handle the buffer to free
 */
void free_flow(flow_manager_t *flow) {
        struct list_head *cur;
        struct list_head *old;
        struct list_head *head;
        data_segment_t *cur_seg;

        head = &(flow->head);
        old = NULL;

        list_for_each(cur, head) {
                if (old) list_del(old);
                cur_seg = list_entry(cur, data_segment_t, list);
                free_data_segment(cur_seg);
                old = cur;
        }

        mutex_destroy(&(flow->op_mutex));
        kfree(flow);
        return;
}