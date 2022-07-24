#include "queue.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>


void initialize_queue(FIFO_QUEUE* queue)
{
    assert(queue != NULL);

    queue->head = NULL;
    queue->tail = NULL;
}


bool one_element_in_queue(FIFO_QUEUE* queue) 
{
    return (queue->head == queue->tail);
}

bool queue_empty(FIFO_QUEUE* queue) 
{
    return (queue->head == NULL && queue->tail == NULL);
}

// Assumptions: thread_id is value and queue is valid
void push_back(FIFO_QUEUE* queue, int thread_id)
{
    FIFO_NODE* node = malloc(sizeof(FIFO_NODE));
    node->thread_id = thread_id;
    node->next = NULL;

    if (queue->head == NULL && queue->tail == NULL)
    {
        // The queue is empty, so make both head and tail point to 
        // the new node
        queue->head = node;
        queue->tail = node;
    }
    else if (queue->head != NULL && queue->tail != NULL)
    {
        // The queue is not empty
        queue->tail->next = node;
        queue->tail = node;
    }
    else
    {
        // Should never happen
        assert(0);
    }
}

// returns if the operation succeeded. It would fail if there are no nodes in the queue
bool pop_front(FIFO_QUEUE* queue, int* thread_id)
{
    if (queue->head == NULL && queue->tail == NULL)
    {
        return false;
    }
    else if (queue->head != NULL && queue->tail != NULL)
    {
        FIFO_NODE* new_front = queue->head->next;
        *thread_id = queue->head->thread_id;
        if (new_front == NULL) 
        {
            // This is the last one
            queue->tail = NULL;
        }
        free(queue->head);
        queue->head = new_front;
        return true;
    }
    else
    {
        assert(0);
    }
}

// different from pop_front, this one requires you knowing which node 
// you want to remove, and it will be removed from queue. 
bool remove_node(FIFO_QUEUE* queue, int thread_id)
{
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
        pop_front(queue, &thread_id);
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

// returns if the queue is not empty
bool move_front_to_back(FIFO_QUEUE* queue, int* thread_id)
{
    if (queue->head == NULL && queue->tail == NULL)
    {
        return false;
    }
    else if (one_element_in_queue(queue))
    {
        // The queue only has 1 node, so we don't need to change anything
        *thread_id = queue->head->thread_id;
        return true;
    }
    else if (queue->head != NULL && queue->tail != NULL)
    {

        FIFO_NODE* new_front = queue->head->next;
        queue->tail->next = queue->head;
        queue->tail = queue->head;
        queue->head = new_front;
        queue->tail->next = NULL;

        
        *thread_id = queue->head->thread_id;
        assert(queue->head != NULL && queue->tail != NULL);
        return true;
    }
    else
    {
        assert(0);
    }
}

// Note: does not free the queue itself, because it's assuming that the queue
// is not dynamically allocated. 
void delete_queue(FIFO_QUEUE* queue)
{
    bool successful_pop = true;
    while (successful_pop)
    {
        int temp;
        successful_pop = pop_front(queue, &temp);
    }
}



bool peek_queue (FIFO_QUEUE* queue, int* thread_id)
{
    if (queue->head == NULL && queue->tail == NULL)
    {
        return false;
    }
    else
    {
        *thread_id = queue->head->thread_id;
        return true;
    }
}

// Assumes requested_thread_id exists in queue
bool move_selected_thread_to_front(FIFO_QUEUE* queue, int requested_thread_id)
{
    int temp;
    if (move_front_to_back(queue, &temp))
    {
        FIFO_NODE* curr = queue->head;
        FIFO_NODE* prev = NULL;
        while (curr != NULL && curr->thread_id != requested_thread_id) {
            prev = curr;
            curr = curr->next;
        }

        assert(curr != NULL); // if curr is null, then requested_thread_id isn't found in queue

        if (prev == NULL)
        {
            // requested ID is also the first in the list. 
            // No-op
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
            curr->next = queue->head;
            queue->head = curr;
        }
        
        return true;
    }
    return false;
}
