#include "thread.h"
#include "test_thread.h"
#include "stdio.h"


void x(void* none) {
	while (1) {
		printf("lol\n");
	}
}

int
main(int argc, char **argv)
{
	thread_init();
	test_basic();

	return 0;
}
