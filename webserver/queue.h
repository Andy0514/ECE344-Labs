#ifndef _QUEUE_H_
#define _QUEUE_H_
// This implements a FIFO queue using linked list by keeping track of a head and tail pointer
// that points to the first and last elements respectively
// The queue stores the thread ID, which is an int
#include <stdbool.h>

typedef struct request_node {
    struct request_node* next;
    int connfd;
} REQUEST_NODE;



/* This is the wait queue structure */
typedef struct request_queue {
	// Identical to READY_QUEUE. I hate C...
    REQUEST_NODE* head; // points to the first node
    REQUEST_NODE* tail; // points to the last node
    int max_size;
    int curr_size;
} REQ_QUEUE;


REQ_QUEUE* create_request_queue(int size);

bool push_back(REQ_QUEUE* queue, int connfd);

bool pop_front(REQ_QUEUE* queue, int* connfd);


void delete_queue(REQ_QUEUE* queue);


#endif