#include "common.h"


int recursive_factorial(int num)
{
	if (num == 1)
	{
		return num;
	}
	else
	{
		return num * recursive_factorial(num-1);
	}
}

int main(int argc, char** argv)
{
	const char* error = "Huh?\n";
	if (argc == 2)
	{
		char* endptr;
		const char* input = argv[1];
		long num = strtol(input, &endptr, 0);
		if (*endptr == '\0' && num > 0)
		{
			// the input was a valid long integer
			if (num > 12)
			{
				printf("Overflow\n");
			}
			else
			{
				printf("%d\n", recursive_factorial((int)num));
			}
		}
		else
		{
			printf("%s", error);
		}
	}
	else
	{
		printf("%s", error);
	}
	
	return 0;
}
