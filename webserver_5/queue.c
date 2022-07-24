#include "queue.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "common.h"


REQ_QUEUE* create_request_queue(int size)
{
    REQ_QUEUE* request_queue = Malloc(sizeof(REQ_QUEUE));
    request_queue->head = NULL;
    request_queue->tail = NULL;
    request_queue->max_size = size;
    request_queue->curr_size = 0;
    return request_queue;
}


bool queue_empty(REQ_QUEUE* queue) 
{
    return (queue->head == NULL && queue->tail == NULL);
}


void push_back_helper(REQUEST_NODE** head, REQUEST_NODE** tail, int connfd){
    REQUEST_NODE* node = (REQUEST_NODE*) Malloc(sizeof(REQUEST_NODE));
    assert(node);
    node->connfd = connfd;
    node->next = NULL;

    if (*head == NULL && *tail == NULL)
    {
        // The queue is empty, so make both head and tail point to 
        // the new node
        *head = node;
        *tail = node;
    }
    else if (*head != NULL && *tail != NULL)
    {
        // The queue is not empty
        (*tail)->next = node;
        *tail = node;
    }
    else
    {
        // Should never happen
        assert(0);
    }
}

// Assumptions: thread_id is value and queue is valid
// assume lock is acquired
bool push_back(REQ_QUEUE* queue, int connfd)
{
    if (queue->curr_size < queue->max_size){
        push_back_helper(&(queue->head), &(queue->tail), connfd);
        queue->curr_size++;
        return true;
    }
    return false;
}


bool pop_front_helper(REQUEST_NODE** head, REQUEST_NODE** tail, int* connfd){
    assert (*head != NULL && *tail != NULL);

    REQUEST_NODE* new_front = (*head)->next;
    *connfd = (*head)->connfd;
    if (new_front == NULL) 
    {
        // This is the last one
        *tail = NULL;
    }
    free(*head);
    *head = new_front;
    return true;
}

// returns if the operation succeeded. It would fail if there are no nodes in the queue
// assume lock is acquired
bool pop_front(REQ_QUEUE* queue, int* connfd)
{
    if (queue->curr_size > 0) {
        pop_front_helper(&(queue->head), &(queue->tail), connfd);
        queue->curr_size--;
        return true;
    }
    else
    {
        return false;
    }
}


void delete_queue(REQ_QUEUE* queue)
{
    bool successful_pop = true;
    while (successful_pop)
    {
        int temp;
        successful_pop = pop_front(queue, &temp);
    }
    free(queue);
}


FILESIZE_QUEUE* create_filesize_queue() {
    FILESIZE_QUEUE* queue = Malloc(sizeof(FILESIZE_QUEUE));
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}

void insert_into_filesize_queue(FILESIZE_QUEUE* queue, int filesize, char* filename)
{
    FILESIZE_NODE* node = (FILESIZE_NODE*) Malloc(sizeof(FILESIZE_NODE));
    assert(node);
    node->filesize = filesize;
    node->filename = Malloc(sizeof(char) * (strlen(filename) + 1));
    strcpy(node->filename, filename);
    node->next = NULL;

    if (queue->head == NULL && queue->tail == NULL)
    {
        // first entry
        queue->head = node;
        queue->tail = node;
    }
    else if (queue->head != NULL && queue->tail != NULL)
    {
        // insert by descending filesize order
        FILESIZE_NODE* curr_node = queue->head;
        FILESIZE_NODE* prev_node = NULL;
        while (curr_node != NULL) {
            if (filesize > curr_node->filesize)
                break;
            prev_node = curr_node;
            curr_node = curr_node->next;
        }

        if (curr_node == NULL) {
            // this is the smallest so far. Insert at the back
            prev_node->next = node;
            queue->tail = node;
        }
        else if (prev_node == NULL) {
            // this is the largest file so far, insert at front
            node->next = queue->head;
            queue->head = node;
        }
        else
        {
            // insert before curr_node
            prev_node->next = node;
            node->next = curr_node;
        }
    }
}


bool pop_front_fq(FILESIZE_QUEUE* queue, char** return_filename, int* popped_file_size)
{
    if (queue->head != NULL && queue->tail != NULL) {
        FILESIZE_NODE* new_head = queue->head->next;
        *return_filename = Malloc(sizeof(char) * (strlen(queue->head->filename) + 1));
        strcpy(*return_filename, queue->head->filename);
        *popped_file_size = queue->head->filesize;
        
        if (new_head == NULL) {
            queue->tail = NULL;
        }
        free(queue->head->filename);
        free(queue->head);
        queue->head = new_head;
        return true;
    }
    return false;
}


void delete_queue_fq(FILESIZE_QUEUE* queue) {
    FILESIZE_NODE* curr_node = queue->head;
    while (curr_node != NULL)
    {
        FILESIZE_NODE* next_node = curr_node->next;
        free(curr_node->filename);
        free(curr_node);
        curr_node = next_node;
    }
    free(queue);
}