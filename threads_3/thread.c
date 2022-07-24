/*
Open questions remaining: 
1. How to determine stack size, and if a stack has exceeded THREAD_MIN_STACK?
2. In thread_kill, should I exit immediately upon yielding to the killed thread, or let the killed thread run then yield? Assuming
	the first because of test_wait_kill test
3. In thread_sleep, is it valid if the only remaining thread is a thread marked for kill?
4. How should thread_exit behave when there are only killed threads left? Only sleeping threads left?
*/

/* Answers to part 3 questions: 
1. SIGALRM - alarm signal triggered from setitimer syscall
2. raise() is used to send signal to self
3. signal happens once every 200us
4. thread_yield is invoked, which yields the CPU to another thread
5. signal is disabled in the handler, including thread_yield, because the signal that caused the interrupt is disabled by default
	Other than that, the block mask (sa_mask) is empty, so other signal still can interrupt. No code reenables it - its reenabled 
	automatically upon exiting the handler
6. printf is a non-reentrant function. Reentrant functions are those that can be interrupted midway and still complete without problems
	By disabling interrupts in printf, it ensures that printf can succeed

Things to note: 
1. interrupts are enabled/disabled on a per-thread basis, and that is saved in each thread's context's uc_sigmask. Therefore, 
*/
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include "queue.h"

// These are all the states needed for lab 2
// More will be added for lab 3
typedef enum THREAD_STATE{
	NOT_INITIALIZED = 0,
	RUNNING,
	READY,
	EXITED,
	KILLED,
	SLEEPING
} THREAD_STATE;

/* This is the thread control block */
struct thread {
	ucontext_t context;
	void* orig_stack_begin; // the address returned by malloc. This is used to free the stack
							// later on. It needs to adjust to satisfy the byte alignment
	THREAD_STATE state;
	struct wait_queue wqueue;
};

//////////////////////
/* Global Variables */
//////////////////////
// thread_array uses thread ID index to access context
struct thread thread_array[THREAD_MAX_THREADS];
READY_QUEUE thread_queue;
int running_thread_id;
bool thread_exit_cleanup_needs_to_run = false;


/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
	interrupts_on();
	thread_main(arg); // call thread_main() function with arg
	thread_exit();
}




// Assumes that this will only be run by 1 thread (the thread system hasn't been set up yet)
void thread_init(void)
{
	// Initializes thread_array
	for (int i = 0; i < THREAD_MAX_THREADS; i++)
	{
		thread_array[i].orig_stack_begin = NULL;
		thread_array[i].state = NOT_INITIALIZED;
	}

	// set up the main thread (ID=0). The context is already obtained from above
	assert(getcontext(&thread_array[0].context) != -1);
	thread_array[0].state = RUNNING;

	// We probably shouldn't deallocate the first stack because it wasn't allocated by us
	// so leave orig_stack_begin as NULL

	running_thread_id = 0;

	initialize_queue_rq(&thread_queue);
}

Tid thread_id()
{
	return running_thread_id;
}

Tid thread_create(void (*fn) (void *), void *parg)
{
	int prev_sig_stat = interrupts_set(0); // BEGIN CRITICAL SECTION
	thread_exit_cleanup();

	// Find an empty thread ID
	int thread_id = THREAD_NONE;
	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
		if (thread_array[i].state == NOT_INITIALIZED) {	
			thread_array[i].state = READY;

			// Important: rather than initializing the context from zero, 
			// initialize from the current context at creation time. If I init the context
			// at thread_init, there would be segfault. Why? Dunno. Must find out. 
			// Also, this would set the interrupt of the new thread to off, so I must 
			// turn the interrupt on in the stub function 
			getcontext(&thread_array[i].context);
			thread_id = i;
			break;
		}
	}

	if (thread_id == THREAD_NONE) {
		interrupts_set(prev_sig_stat); // returning, so reset the interrupt 
		return THREAD_NOMORE;
	}

	ucontext_t* context = &thread_array[thread_id].context;

	// Allocate a stack using malloc. stack is the lower bound
	void* stack = malloc(THREAD_MIN_STACK);
	if (stack == NULL) {
		thread_array[thread_id].state = NOT_INITIALIZED;
		interrupts_set(prev_sig_stat); // returning, so reset the interrupt 
		return THREAD_NOMEMORY;
	}

	thread_array[thread_id].orig_stack_begin = stack;

	void* upper_bound = stack + (THREAD_MIN_STACK);

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
	context->uc_stack.ss_size = THREAD_MIN_STACK;
	context->uc_stack.ss_flags = 0;
	context->uc_link = NULL;

	push_back_rq(&thread_queue, thread_id);
	
	interrupts_set(prev_sig_stat); // END CRITICAL SECTION
	return thread_id;
}

Tid
thread_yield(Tid want_tid)
{
	// pretty much nothing here is thread safe...
	int prev_sig_stat = interrupts_set(0); // BEGIN CRITICAL SECTION
	int current_tid = thread_id();
	int new_tid = (want_tid == THREAD_SELF) ? current_tid : want_tid;

	THREAD_STATE* curr_tid_state = &thread_array[current_tid].state;

	if (want_tid == THREAD_ANY || new_tid == current_tid || 
		(want_tid >= 0 && want_tid < THREAD_MAX_THREADS && 
		(thread_array[new_tid].state == READY || thread_array[new_tid].state == KILLED)))
	{
		// move the current thread onto queue if it's not in an invalid state
		if (*curr_tid_state != EXITED && *curr_tid_state != SLEEPING) {
			push_back_rq(&thread_queue, current_tid);
		}

		if (want_tid == THREAD_ANY)
		{
			// get the next thread to run in new_tid
			// there should never be no nodes in queue because of push_back above,
			// and an exited thread should terminate if it's the last
			assert(pop_front_rq(&thread_queue, &new_tid)); 
			if (new_tid == current_tid) 
			{
				interrupts_set(prev_sig_stat);
				return THREAD_NONE;
			}
		}
		else
		{
			assert(remove_node_rq(&thread_queue, new_tid));
		}

		assert(new_tid >= 0);
		running_thread_id = new_tid;
		
		// actual switching
		volatile bool set_context_called = false;
		assert(getcontext(&thread_array[current_tid].context) >= 0);

		if (!set_context_called) {
			assert(!interrupts_enabled());
			
			set_context_called = true;
			struct ucontext_t* new_context = &thread_array[new_tid].context;

			if (thread_array[new_tid].state == KILLED) 
			{
				// the next thread is marked for exit, so change RIP to point to  
				// the exit function, which will set the state as EXITED
				new_context->uc_mcontext.gregs[REG_RIP] = (long long int) &thread_exit;
			} 
			
			*curr_tid_state = (*curr_tid_state == EXITED || *curr_tid_state == SLEEPING) ? *curr_tid_state : READY;
			thread_array[new_tid].state = RUNNING;

			assert(setcontext(new_context) != -1); // context switch. A successful call doesn't return
		}

		interrupts_set(prev_sig_stat);
		return new_tid;
	}

	interrupts_set(prev_sig_stat);
	return THREAD_INVALID;
}
		

void thread_exit()
{
	//disables interrupts because all the queue functions 
	// are not thread-safe: a context switch right after checking 
	// queue empty into a thread_create may make the program preemptively exit
	// However, because this thread is marked for exit, no need to reenable 
	// interrupts
	int prev_sig_stat = interrupts_set(0);

	int curr_tid = thread_id();
	
	// clean up any other threads that are marked as exited
	thread_exit_cleanup();

	// mark this thread as exited so that it will be cleaned up on subsequent exits
	thread_array[curr_tid].state = EXITED;
	thread_exit_cleanup_needs_to_run = true;

	// relaunch threads that are waiting for this thread to exit
	thread_wakeup(&thread_array[curr_tid].wqueue, 1);

	if (!queue_empty_rq(&thread_queue))
	{
		interrupts_set(prev_sig_stat);
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
	// must be run with interrupts disabled because it accesses many global data
	assert(!interrupts_enabled());

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


// Assuming that thread_kill can kill a sleeping thread - when it next wakes up, it will be put into
// the ready queue for kill
Tid thread_kill(Tid tid)
{
	int prev_sig_stat = interrupts_set(0);
	// First go through all threads and determine if any needs to be set to
	// NOT_INITIALIZED
	thread_exit_cleanup();

	// Mark the thread as killed. It will be marked as exited during 
	// the next thread_yield. Note: cannot kill self!
	if (tid < 0 || tid >= THREAD_MAX_THREADS || tid == thread_id() || thread_array[tid].state == NOT_INITIALIZED) {
		interrupts_set(prev_sig_stat);
		return THREAD_INVALID;
	}
	thread_array[tid].state = KILLED;

	interrupts_set(prev_sig_stat);

	return tid;
}


bool ready_queue_empty() {
	
	int prev_sig_stat = interrupts_set(0);
	bool result = queue_empty_rq(&thread_queue);
	interrupts_set(prev_sig_stat);
	return result;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	
	int prev_sig_stat = interrupts_set(0);
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	initialize_queue_wq(wq);
	
	interrupts_set(prev_sig_stat);
	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	int prev_sig_stat = interrupts_set(0);
	delete_queue_wq(wq);
	free(wq);
	interrupts_set(prev_sig_stat);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	int prev_sig_stat = interrupts_set(0);

	if (queue == NULL) {
		interrupts_set(prev_sig_stat);
		return THREAD_INVALID;
	}

	if (queue_empty_rq(&thread_queue)) {
		// This is the only remaining running thread, so it shouldn't sleep
		interrupts_set(prev_sig_stat);
		return THREAD_NONE;
	}

	// add this thread to the sleep queue
	push_back_wq(queue, thread_id());

	thread_array[thread_id()].state = SLEEPING;
	int next_thread = thread_yield(THREAD_ANY);
	
	interrupts_set(prev_sig_stat);
	return next_thread;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(WAIT_QUEUE *queue, int all)
{
	int prev_sig_stat = interrupts_set(0);
	int num_threads_woken_up = 0;
	if (queue && !queue_empty_wq(queue) && (all == 0 || all == 1)) {

		// wake up threads
		while (!queue_empty_wq(queue)) {
			int woken_thread;
			assert(pop_front_wq(queue, &woken_thread));
			push_back_rq(&thread_queue, woken_thread);
			// This allows threads to be killed while sleeping, and doesn't 
			// change the state back to ready state
			if (thread_array[woken_thread].state == SLEEPING) {
				thread_array[woken_thread].state = READY;
			}
			num_threads_woken_up++;

			if (all == 0) break; // terminate after the 1st iteration if
								 // we only want to pop the first thread
		}
	}
	interrupts_set(prev_sig_stat);
	return num_threads_woken_up;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid)
{
	int prev_sig_stat = interrupts_set(0);
	if (tid < 0 || tid >= THREAD_MAX_THREADS || thread_array[tid].state == NOT_INITIALIZED || tid == thread_id()) {
		interrupts_set(prev_sig_stat);
		return THREAD_INVALID;
	}

	thread_sleep(&thread_array[tid].wqueue);
	interrupts_set(prev_sig_stat);
	return tid;
}

struct lock {
	struct wait_queue wqueue;
	bool acquired;
	int acquiring_thread;
};

// no exclusion needed because everything here is local
// the lock isn't shared until this returns
struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	lock->acquired = false;
	lock->acquiring_thread = THREAD_NONE;

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	int prev_sig_stat = interrupts_set(0);
	assert(lock != NULL);
	assert(!lock->acquired); // lock must be free when destroyed
	assert(queue_empty_wq(&lock->wqueue));
	free(lock);
	interrupts_set(prev_sig_stat);
}

void
lock_acquire(struct lock *lock)
{
	int prev_sig_stat = interrupts_set(0);
	assert(lock != NULL);

	while (lock->acquired) {
		thread_sleep(&lock->wqueue);
	}

	// this thread is free to acquire the lock
	lock->acquired = true;
	lock->acquiring_thread = thread_id();
	interrupts_set(prev_sig_stat);
}

void
lock_release(struct lock *lock)
{
	int prev_sig_stat = interrupts_set(0);
	assert(lock != NULL);
	assert(lock->acquiring_thread == thread_id());
	lock->acquired = false;

	// wake up all threads waiting for this lock
	thread_wakeup(&lock->wqueue, 1);
	
	interrupts_set(prev_sig_stat);
}

struct cv {
	/* ... Fill this in ... */
	struct wait_queue wqueue;

};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	int prev_sig_stat = interrupts_set(0);
	assert(cv != NULL); 
	assert(queue_empty_wq(&cv->wqueue));

	free(cv);
	
	interrupts_set(prev_sig_stat);
}

// This always waits, so caller must be careful to avoid 
// signal loss. This returns immediately after the condition
// has been met, so the caller must check that after several threads
// have been run, the condition is still correct
void cv_wait(struct cv *cv, struct lock *lock)
{
	// probably don't need to disable interrupts here, 
	// because the lock is held throughout, and a context switch
	// around thread_sleep won't be too harmful

	assert(cv != NULL);
	assert(lock != NULL);
	
	int prev_sig_stat = interrupts_set(0);
	assert(lock->acquiring_thread == thread_id());
	interrupts_set(prev_sig_stat);

	lock_release(lock);
	thread_sleep(&cv->wqueue);

	// now, the thread has woken up, so try to acquire a lock.
	// if many threads wake up at the same time, only 1 will successfully 
	// acquire the lock at this time. The calling function must be able to 
	// recheck the condition, and rewait the remaining threads on this CV
	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
	
	int prev_sig_stat = interrupts_set(0);
	assert(lock->acquiring_thread == thread_id());
	interrupts_set(prev_sig_stat);

	thread_wakeup(&cv->wqueue, 0);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	int prev_sig_stat = interrupts_set(0);
	assert(lock->acquiring_thread == thread_id());
	interrupts_set(prev_sig_stat);

	thread_wakeup(&cv->wqueue, 1);
}
