// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-entry+1023)/1024);
	return 0;
}

int
mon_time(int argc, char **argv, struct Trapframe *tf)
{
        uint64_t start = 0;
	uint64_t end = 0;
	if (argc == 1) {
		cprintf("Usage: time [command]\n");
		return 0;
	}
	int i = 0;
	for (; i < NCOMMANDS; i++) {
		if (strcmp(argv[1], commands[i].name) == 0) {
			start = read_tsc();
			commands[i].func(argc-1,argv+1,tf);
			end = read_tsc();
			cprintf("%s cycles: %llu\n", argv[1], end-start);
			return 0;
		}
	}
	return 0;

}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

    char str[256] = {};
    int nstr = 0;
    char *pret_addr;
    uint32_t pcall_addr = (uint32_t)do_overflow+3;
    pret_addr = (char *)read_pretaddr();
    int i = 0;
    uint32_t j = 0;
    for(i; i < 256; i++) {
	str[i] = 'a';
    }
    for(j; j < 4; j++) {
	nstr = (pcall_addr >> (8*j)) & 0xFF;
//	cprintf("%d: %0x\n",j,nstr);
	str[nstr] = '\0';
	cprintf("%s%n",str,pret_addr+j);
	str[nstr] = 'a';
    }
    
	// Your code here.
    


}

void
overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
    uint32_t ebp = read_ebp();
    uint32_t eip = read_eip();
    cprintf("Stack backtrace:\n");
    while(ebp != 0x0){
	eip = *((uint32_t *)ebp + 1);
	uint32_t tmp = ebp;
	cprintf("  eip %08x  ebp %08x  args %08x %08x %08x %08x %08x\n",eip,ebp,*((uint32_t *)tmp+2),*((uint32_t *)tmp+3),*((uint32_t *)tmp+4),*((uint32_t *)tmp+5),*((uint32_t *)tmp+6));
	struct Eipdebuginfo info;
	if(debuginfo_eip(eip, &info) == 0) {
	    char buff[info.eip_fn_namelen+1];
            buff[info.eip_fn_namelen] = '\0';
	    int i = -1;
	    while( info.eip_fn_namelen > i++) {
		buff[i] = info.eip_fn_name[i];
	    }
	    cprintf("         %s:%d: %s+%x\n", info.eip_file, info.eip_line,buff,eip-info.eip_fn_addr);
	}	
	ebp = *((uint32_t *)ebp); // next stack
    }
    overflow_me();
    cprintf("Backtrace success\n");
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
