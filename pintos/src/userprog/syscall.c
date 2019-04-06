#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <list.h>

static void syscall_handler (struct intr_frame *);
static struct semaphore sema_file;
void s_halt(void);
void s_exit(int status);
tid_t s_exec(char * cmd_line);
int s_wait(tid_t t);
bool s_create(const char *file, unsigned initial_size);
bool s_remove(const char *file);
int s_open(const char * file);
int s_filesize(int fd);
int s_read(int fd, void * buffer, unsigned size);
int s_write(int fd, const void * buffer, unsigned size);
void s_seek(int fd, unsigned position);
unsigned s_tell(int fd);
void s_close(int fd);
struct file_block * get_fb_fd(int fd);

	

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
	sema_init(&sema_file, 1);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

	void *p = f->esp;
	//hex_dump(p, p, 100, 1);	
	//printf("%d %p\n",*(int*)(f->esp),f->esp);
	//hex_dump(f->esp, f->esp, 100, 1);
  //printf ("system call!\n");	
	//printf("%d\n",*(int *)(f->esp));
	//thread_exit();
	//printf("system call!\n");
	//printf("%d, %d, %d, %d\n",*(int *)(p),(p+4),*(int *)(p+8), *(int *)(p+12));
	if(!is_user_vaddr(p+4) || !is_user_vaddr(p+8) || !is_user_vaddr(p+12))
	{	
		s_exit(-1);
	}
		//thread_exit();
	switch(*(int *)p)
	{
		int len;	
		case SYS_HALT:
			s_halt();
			break;
		case SYS_EXIT:
			s_exit(*(int *)(p+4));
			break;
		case SYS_EXEC:	
			f->eax = s_exec(*(int *)(p+4));
			break;
		case SYS_WAIT:
			f->eax = s_wait(*(int *)(p+4));
			break;
		case SYS_CREATE:
			f->eax = s_create(*(int *)(p+4),(unsigned)*(int *)(p+8));
			break;
		case SYS_REMOVE:
			f->eax = s_remove(*(int *)(p+4));
			break;
		case SYS_OPEN:
			f->eax = s_open(*(int *)(p+4));
			break;
		case SYS_FILESIZE:
			f->eax = s_filesize(*(int *)(p+4));
			break;
		case SYS_READ:
			f->eax = s_read(*(int*)(p+4), *(int*)(p+8),(unsigned)*(int *)(p+12));
			break;
		case SYS_WRITE:
			f->eax=s_write(*(int*)(p+4),*(int*)(p+8),(unsigned)*(int *)(p+12));
			break;
		case SYS_SEEK:
			s_seek(*(int*)(p+4),(unsigned)*(int *)(p+8));
			break;
		case SYS_TELL:
			f->eax = s_tell(*(int *)(p+4));
			break;
		case SYS_CLOSE:
			s_close(*(int *)(p+4));
			break;
	}
		
}

void
s_halt(void)
{
	power_off();
}

void
s_exit(int status)
{
	printf("%s: exit(%d)\n",thread_name(),status);
	thread_current()->exit_code = status;
	thread_exit();
	
}

tid_t
s_exec(char * cmd_line)
{
	int pid;
	if(cmd_line == NULL)
		return -1;
	pid = process_execute(cmd_line);
	return pid;
}

int 
s_wait(tid_t t)
{
	int e_code = process_wait(t);
	
	return e_code;
}

bool 
s_create(const char *file, unsigned initial_size)
{
	bool success;
	if(file == NULL)
		s_exit(-1);
	success = filesys_create(file, initial_size);
	return success;
}

bool 
s_remove(const char *file)
{
	bool success = filesys_remove(file);
	return success;
}

int 
s_open(const char * file)
{
	struct file *of;
	struct file_block *fb;
	struct thread * c_thread;
	struct list *f_list;
	
	if(file==NULL)
		return -1;
	
	of = filesys_open(file);
	if(of==NULL)
		return -1;

	fb = palloc_get_page(0);
	if(fb==NULL)
	{
		palloc_free_page(fb);
		return -1;
	}
	c_thread = thread_current();
	fb->f = of;
	fb->fd = c_thread->next_fd;
	c_thread->next_fd += 1;

	list_push_back(&c_thread->file_list, &fb->flist);
	
	return fb->fd;
}

int 
s_filesize(int fd)
{
	struct file_block * fb;
	
	fb = get_fb_fd(fd);
	if(fd == NULL)
		return 0;
	else
		return file_length(fb->f);
}

int 
s_read(int fd, void * buffer, unsigned size)
{
	int len =0;
	struct file_block * fb;
	if(fd ==0)
		return len;
	if(fd ==1)
		return 0;
	fb = get_fb_fd(fd);

	len = file_read(fb->f, buffer, size);
	return len;
}

int 
s_write(int fd, const void * buffer, unsigned size)
{
	int len;
	struct file_block * fb;
	if(fd == 1)
	{	
		putbuf(buffer, size);
		return size;
	}
	
	if(buffer == NULL)
		return 0;
	fb = get_fb_fd(fd);

	len = file_write(fb->f, buffer, (int32_t)size);
	
	return len;
}

void 
s_seek(int fd, unsigned position)
{
}

unsigned 
s_tell(int fd)
{
	return 1;
}

void 
s_close(int fd)
{
	struct file_block *fb;
	fb = get_fb_fd(fd);

	if(fb == NULL)
		return;
	file_close(fb->f);
	palloc_free_page(fb);

}

struct file_block *
get_fb_fd(int fd)
{
	struct thread * c_thread;
	struct list_elem *temp, *end;
	struct file_block * fb;
	c_thread = thread_current();

	temp = list_begin(&c_thread->file_list);
	end = list_end(&c_thread->file_list);

	while(temp != end)
	{
		fb = list_entry(temp, struct file_block, flist);
		if(fb->fd == fd)
			return fb;
		temp = list_next(temp);
	}
	
	return NULL;

}

