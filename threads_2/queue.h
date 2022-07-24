// This implements a FIFO queue using linked list by keeping track of a head and tail pointer
// that points to the first and last elements respectively
// The queue stores the thread ID, which is an int
#include <stdbool.h>

typedef struct FIFO_NODE {
    struct FIFO_NODE* next;
    int thread_id;
} FIFO_NODE;

typedef struct FIFO_QUEUE {
    FIFO_NODE* head; // points to the first node
    FIFO_NODE* tail; // points to the last node

    // Note: if head == tail == NULL, the queue is empty
    // If head == tail != NULL, there's only 1 node in the queue
} FIFO_QUEUE;

void initialize_queue(FIFO_QUEUE* queue);
void push_back(FIFO_QUEUE* queue, int thread_id);

// returns if the operation succeeded. It would fail if there are no nodes in the queue
// This returns the popped thread ID as argument
bool pop_front(FIFO_QUEUE* queue, int* thread_id);

// This function moves the frontmost node to the backmost position
// This partially implements the yield behaviour. It returns the thread ID
//  that is now at the front of the queue as argument
bool move_front_to_back(FIFO_QUEUE* queue, int* thread_id);

// This function moves the node indicated by requested_thread_id to the front 
// of the queue, and moves the old front of the queue to the back of the queue
// Assumes that requested_thread_id is valid and exists in the queue
bool move_selected_thread_to_front(FIFO_QUEUE* queue, int thread_id);

void delete_queue(FIFO_QUEUE* queue);

// This function returns the value of the first node in queue
// (the running thread) without removing it in the thread_id arg.
// Returns if the queue is not empty as bool
bool peek_queue (FIFO_QUEUE* queue, int* thread_id);


bool one_element_in_queue(FIFO_QUEUE* queue);


bool queue_empty(FIFO_QUEUE* queue);

bool remove_node(FIFO_QUEUE* queue, int thread_id);
