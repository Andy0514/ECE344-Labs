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

