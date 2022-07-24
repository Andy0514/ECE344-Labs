/*
Open questions remaining: 
1. How to determine stack size, and if a stack has exceeded THREAD_MIN_STACK?
*/
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include "queue.h"

/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
	thread_main(arg); // call thread_main() function with arg
	thread_exit();
}


/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
};

// These are all the states needed for lab 2
// More will be added for lab 3
typedef enum THREAD_STATE{
	NOT_INITIALIZED = 0,
	RUNNING,
	READY,
	EXITED,
	KILLED
} THREAD_STATE;

/* This is the thread control block */
struct thread {
	ucontext_t context;
	void* orig_stack_begin; // the address returned by malloc. This is used to free the stack
							// later on. It needs to adjust to satisfy the byte alignment
	THREAD_STATE state;
};

// thread_array uses thread ID index to access context
struct thread thread_array[THREAD_MAX_THREADS];
FIFO_QUEUE thread_queue;
int running_thread_id;
bool thread_exit_cleanup_needs_to_run = false;

void thread_init(void)
{
	// Initializes thread_array
	for (int i = 0; i < THREAD_MAX_THREADS; i++)
	{
		// Important: rather than initializing the context from zero, 
		// initialize from the current context. Why? Dunno. 
		assert(getcontext(&thread_array[i].context) >= 0); 
		thread_array[i].orig_stack_begin = NULL;
		thread_array[i].state = NOT_INITIALIZED;
	}

	// set up the main thread (ID=0)
	int err = getcontext(&thread_array[0].context);
	assert(!err);
	thread_array[0].state = RUNNING;

	// We probably shouldn't deallocate the first stack because it wasn't allocated by us
	// so leave orig_stack_begin as NULL

	running_thread_id = 0;
}

Tid thread_id()
{
	return running_thread_id;
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
	thread_exit_cleanup();

	// Find an empty thread ID
	int thread_id = THREAD_NONE;
	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
		if (thread_array[i].state == NOT_INITIALIZED) {
			thread_id = i;
			thread_array[i].state = READY;
			break;
		}
	}
	if (thread_id == THREAD_NONE) {
		return THREAD_NOMORE;
	}

	ucontext_t* context = &thread_array[thread_id].context;

	// Allocate a stack using malloc. stack is the lower bound
	void* stack = malloc(THREAD_MIN_STACK);
	if (stack == NULL) {
		thread_array[thread_id].state = NOT_INITIALIZED;
		return THREAD_NOMEMORY;
	}

	thread_array[thread_id].orig_stack_begin = stack;

	void* upper_bound = stack + THREAD_MIN_STACK;

	// Find the appropriate initial location for stack pointer. 
	// Because the stack pointer initially points to 8 bytes above RBP, it has to be 
	// 8 bytes out of alignment in order to align RBP with 16 byte boundary. How much
	// offset RSP (pointing to where return address is stored) is from upper_bounds is
	// given by return_address_offset
	int return_address_offset = (long long int) upper_bound % 16;
	if (return_address_offset < 8) {
		return_address_offset = 8 + return_address_offset;
	} else {
		return_address_offset = return_address_offset - 8;
	}
	void* rip_addr = upper_bound - return_address_offset;

	// test that rip_addr is within bounds and is 8 from 16 byte alignment
	assert(rip_addr <= upper_bound && ((long long int)rip_addr - 8)%16 == 0);
	
	// set the appropriate CPU registers
	context->uc_mcontext.gregs[REG_RIP] = (long long int) &thread_stub;
	context->uc_mcontext.gregs[REG_RSP] = (long long int) rip_addr;
	context->uc_mcontext.gregs[REG_RDI] = (long long int) fn; // arg 1 into thread_stub
	context->uc_mcontext.gregs[REG_RSI] = (long long int) parg; // arg 2 into thread_stub

	// Todo: do I need to set these? 
	context->uc_stack.ss_sp = stack;
	context->uc_stack.ss_size = THREAD_MIN_STACK - 100;

	push_back(&thread_queue, thread_id);
	return thread_id;
}

Tid
thread_yield(Tid want_tid)
{
	int current_tid = thread_id();
	int new_tid = (want_tid == THREAD_SELF) ? current_tid : want_tid;


	if (want_tid == THREAD_ANY || new_tid == current_tid || 
		(want_tid >= 0 && want_tid < THREAD_MAX_THREADS && 
		(thread_array[new_tid].state == READY || thread_array[new_tid].state == KILLED)))
	{
		// move the current thread onto queue if it's not exited
		if (thread_array[current_tid].state != EXITED) {
			push_back(&thread_queue, current_tid);
		}

		if (want_tid == THREAD_ANY)
		{
			// get the next thread to run in new_tid
			// there should never be no nodes in queue because of push_back above,
			// and an exited thread should terminate if it's the last
			assert(pop_front(&thread_queue, &new_tid)); 
			if (new_tid == current_tid) 
			{
				return THREAD_NONE;
			}
		}
		else
		{
			assert(remove_node(&thread_queue, new_tid)); 
		}

		assert(new_tid >= 0);
		running_thread_id = new_tid;
		
		
		// actual switching
		volatile bool set_context_called = 0;
		bool success = (getcontext(&thread_array[current_tid].context) >= 0);
		assert(success);

		if (!set_context_called) {
			set_context_called = 1;
			struct ucontext_t* new_context = &thread_array[new_tid].context;

			if (thread_array[new_tid].state == KILLED) 
			{
				// the next thread is marked for exit, so change RIP to point to  
				// the exit function, which will set the state as EXITED
				new_context->uc_mcontext.gregs[REG_RIP] = (long long int) &thread_exit;
			} 
			
			// change the thread states. Don't do so for exited threads because they need to be
			// cleaned up
			thread_array[current_tid].state = (thread_array[current_tid].state == EXITED) ? EXITED : READY;
			thread_array[new_tid].state = RUNNING;
			success &= (setcontext(new_context) != -1); // context switch
			assert(success);
		}
		return new_tid;
	}

	return THREAD_INVALID;
}
		

void thread_exit()
{
	int curr_tid = thread_id();
	
	// First, mark this thread as exited
	// Then, whenever new threads are created, clean-up exited threads
	// This is safe because thread_create must not be called from
	// the current thread (to be killed)
	thread_array[curr_tid].state = EXITED;
	thread_exit_cleanup_needs_to_run = true;
	
	if (!queue_empty(&thread_queue))
	{
		thread_yield(THREAD_ANY);
		assert(0); // should never come here
	}
	else
	{
		// This would exit from the program
		exit(0);
	}
}

void thread_exit_cleanup()
{
	// look through the array for any exited threads, then clean them up 
	// by deallocating their memories
	if (thread_exit_cleanup_needs_to_run)
	{
		for (int i = 0; i < THREAD_MAX_THREADS; i++) {
			if (thread_array[i].state == EXITED) {
				thread_array[i].state = NOT_INITIALIZED;
				free(thread_array[i].orig_stack_begin);
			}
		}
		thread_exit_cleanup_needs_to_run = false;
	}
}



Tid thread_kill(Tid tid)
{
	// First go through all threads and determine if any needs to be set to
	// NOT_INITIALIZED
	thread_exit_cleanup();

	// Mark the thread as killed. It will be marked as exited during 
	// the next thread_yield. Note: cannot kill self!
	if (tid < 0 || tid >= THREAD_MAX_THREADS || tid == thread_id() || thread_array[tid].state == NOT_INITIALIZED) {
		return THREAD_INVALID;
	}
	thread_array[tid].state = KILLED;
	return tid;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}
