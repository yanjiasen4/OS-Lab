// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	//int i = 3;
	cprintf("i: %08x\n",thisenv);
	cprintf("i am environment %08x\n", thisenv->env_id);
}
