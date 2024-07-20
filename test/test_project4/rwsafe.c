#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	assert(argc > 2);
	srand(clock());
	long mem2 = 0;
	uintptr_t mem1 = 0;
	int curs = 0;
	int i;
	sys_move_cursor(2, 2);
	for (i = 0; i < atol(argv[1]); i++)
	{
		mem1 = sys_mmalloc(atol(argv[2]));
		// sys_move_cursor(2, curs+i);
		mem2 = rand();
		*(long*)mem1 = mem2;
		printf("0x%lx, %ld\n", mem1, mem2);
		if (*(long*)mem1 != mem2) {
			printf("Error!\n");
		}
	}
	//Only input address.
	//Achieving input r/w command is recommended but not required.
	printf("Success!\n");
	return 0;
}
