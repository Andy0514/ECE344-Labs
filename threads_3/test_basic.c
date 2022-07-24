#include "thread.h"
#include "test_thread.h"
#include "stdio.h"
#include "interrupt.h"


void x(void* none) {
	while (1) {
		printf("thread %d\n", thread_id());
	}
}

int
main(int argc, char **argv)
{
	//register_interrupt_handler(0);
	thread_init();
	test_basic();

	return 0;
}
