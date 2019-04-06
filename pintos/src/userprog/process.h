#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <list.h>

typedef int pid_t;
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct pcb
{
	pid_t pid;
	struct list_elem child;
	struct semaphore sema_wait;	

};

struct file_block
{
	int fd;
	struct file* f;
	struct list_elem flist;
};

#endif /* :userprog/process.h */
