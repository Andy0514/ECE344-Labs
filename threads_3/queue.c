#include "queue.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "thread.h"
#include "interrupt.h"


void initialize_queue_rq(READY_QUEUE* queue)
{
    assert(queue != NULL);

    queue->head = NULL;
    queue->tail = NULL;
}

void initialize_queue_wq(WAIT_QUEUE* queue)
{
    assert(queue != NULL);

    queue->head = NULL;
    queue->tail = NULL;
}


// bool one_element_in_queue(READY_QUEUE* queue) 
// {
//     assert(!interrupts_enabled());
//     return (queue->head == queue->tail);
// }

bool queue_empty_rq(READY_QUEUE* queue) 
{
    assert(!interrupts_enabled());
    return (queue->head == NULL && queue->tail == NULL);
}

bool queue_empty_wq(WAIT_QUEUE* queue) 
{
    assert(!interrupts_enabled());
    return (queue->head == NULL && queue->tail == NULL);
}


void push_back_helper(FIFO_NODE** head, FIFO_NODE** tail, int thread_id){
    assert(thread_id >= 0 && thread_id < THREAD_MAX_THREADS);
    FIFO_NODE* node = (FIFO_NODE*) malloc(sizeof(FIFO_NODE));
    assert(node);
    node->thread_id = thread_id;
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
void push_back_rq(READY_QUEUE* queue, int thread_id)
{
    assert(!interrupts_enabled());
    push_back_helper(&(queue->head), &(queue->tail), thread_id);
}
void push_back_wq(WAIT_QUEUE* queue, int thread_id)
{
    assert(!interrupts_enabled());
    push_back_helper(&(queue->head), &(queue->tail), thread_id);
}


bool pop_front_helper(FIFO_NODE** head, FIFO_NODE** tail, int*thread_id){
    if (*head == NULL && *tail == NULL)
    {
        return false;
    }
    else if (*head != NULL && *tail != NULL)
    {
        FIFO_NODE* new_front = (*head)->next;
        *thread_id = (*head)->thread_id;
        if (new_front == NULL) 
        {
            // This is the last one
            *tail = NULL;
        }
        free(*head);
        *head = new_front;
        return true;
    }
    else
    {
        assert(0);
    }
}

// returns if the operation succeeded. It would fail if there are no nodes in the queue
bool pop_front_rq(READY_QUEUE* queue, int* thread_id)
{
    assert(!interrupts_enabled());
    return pop_front_helper(&(queue->head), &(queue->tail), thread_id);
}
bool pop_front_wq(WAIT_QUEUE* queue, int* thread_id)
{
    assert(!interrupts_enabled());
    return pop_front_helper(&(queue->head), &(queue->tail), thread_id);
}






// different from pop_front, this one requires you knowing which node 
// you want to remove, and it will be removed from queue. 
bool remove_node_rq(READY_QUEUE* queue, int thread_id)
{
    assert(!interrupts_enabled());
    FIFO_NODE* curr = queue->head;
    FIFO_NODE* prev = NULL;
    while (curr != NULL && curr->thread_id != thread_id) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) return false; // if curr is null, then requested thread_id isn't found in queue

    if (prev == NULL)
    {
        // this node is the first in the queue
        pop_front_rq(queue, &thread_id);
    }
    else
    {
        if (curr->next == NULL) {
            // the one that we want to move is the tail of the queue
            // change the tail to prev. Now, prev shouldn't be null since 
            // this isn't the first in the list
            queue->tail = prev;
        }
        prev->next = curr->next;
        free(curr);
    }
    return true;
}

bool remove_node_wq(WAIT_QUEUE* queue, int thread_id)
{
    assert(!interrupts_enabled());
    FIFO_NODE* curr = queue->head;
    FIFO_NODE* prev = NULL;
    while (curr != NULL && curr->thread_id != thread_id) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) return false; // if curr is null, then requested thread_id isn't found in queue

    if (prev == NULL)
    {
        // this node is the first in the queue
        pop_front_wq(queue, &thread_id);
    }
    else
    {
        if (curr->next == NULL) {
            // the one that we want to move is the tail of the queue
            // change the tail to prev. Now, prev shouldn't be null since 
            // this isn't the first in the list
            queue->tail = prev;
        }
        prev->next = curr->next;
        free(curr);
    }
    return true;
}


// Note: does not free the queue itself, because it's assuming that the queue
// is not dynamically allocated. Really, this shouldn't ever be called because
// the queue should be empty upon program exit
void delete_queue_rq(READY_QUEUE* queue)
{
    assert(!interrupts_enabled());
    bool successful_pop = true;
    while (successful_pop)
    {
        int temp;
        successful_pop = pop_front_rq(queue, &temp);
    }
}

void delete_queue_wq(WAIT_QUEUE* queue)
{
    assert(!interrupts_enabled());
    bool successful_pop = true;
    while (successful_pop)
    {
        int temp;
        successful_pop = pop_front_wq(queue, &temp);
    }
}

