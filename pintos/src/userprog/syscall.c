#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

	void *p = f->esp;
	
	//printf("%d %p\n",*(int*)(f->esp),f->esp);
	//hex_dump(f->esp, f->esp, 100, 1);
  //printf ("system call!\n");	
	//printf("%d\n",*(int *)(f->esp));
	//thread_exit();
	//printf("system call!\n");
	//printf("%d, %d, %d, %d\n",*(int *)(p),(p+4),*(int *)(p+8), *(int *)(p+12));
	
	//thread_exit();
	switch(*(int *)p)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(0);
			break;
		case SYS_EXEC:
			break;
		case SYS_WAIT:
			break;
		case SYS_CREATE:
			break;
		case SYS_REMOVE:
			break;
		case SYS_OPEN:
			break;
		case SYS_FILESIZE:
			break;
		case SYS_READ:
			break;
		case SYS_WRITE:
			write(p);
			break;
		case SYS_SEEK:
			break;
		case SYS_TELL:
			break;
		case SYS_CLOSE:
			break;
	}
		
}

void
halt(void)
{
	power_off();
}

int
exit(int status)
{
	
	printf("%s: exit(%d)\n",thread_name(), status);
	thread_exit();
	return -1;
}

tid_t
exec(char * cmd_line)
{
	int pid;
	pid = process_execute(cmd_line);
}

int
write(void * esp)
{
	int fd = *(int *)(esp+4);
	void * buffer= *(int *)(esp+8);
	int size = *(int *)(esp+12);
	//printf("%x\n",*(int *)(esp+8));
	struct thread * t = thread_current();
	//printf("name : %s\n",t->name);
	if(fd ==1)
	  //printf("ha\n");
		putbuf(buffer, size);
	return size;

}


