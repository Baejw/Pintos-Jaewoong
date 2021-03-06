#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed_point.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif


/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of sleeping thread list. */
static struct list sleep_list;

/* List of all thread */
static struct list LIST;

/* load average */
static int load_avg=0;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;
static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
bool compare_priority (struct list_elem *, struct list_elem *, void *);
/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */

/* This is 2016 spring cs330 skeleton code */

void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&sleep_list);
	list_init (&LIST);
	load_avg = 0;
	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
	
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);
 	 
  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();
	
  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;

  /* Add to run queue. */
  thread_unblock (t);
	if(thread_mlfqs)
	{
		int pri = thread_current()->priority;
		t->priority = pri;
		if(pri<priority)
			thread_yield();
	}
	else
		thread_yield();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
	
  ASSERT (t->status == THREAD_BLOCKED);
	void * aux = NULL;
  list_insert_ordered (&ready_list, &t->elem, compare_priority, aux);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Just set our status to dying and schedule another process.
     We will be destroyed during the call to schedule_tail(). */
  intr_disable ();
  list_remove(&thread_current()->ELEM);
	thread_current ()->status = THREAD_DYING;
  
	schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *curr = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (curr != idle_thread)
	{	
		void * aux = NULL;
		list_insert_ordered (&ready_list, &curr->elem, compare_priority, aux);
	}
		// list_push_back (&ready_list, &curr->elem);
  curr->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  int old_priority = thread_current()->priority;
	struct thread * current_thread = thread_current();
	if(thread_mlfqs)
	{
		return;
	}
	if(current_thread->o_priority==-1)
	{
		current_thread->priority = new_priority;
		if(old_priority > new_priority)
		{
			thread_yield();
		}
	}
	else
	{
		current_thread->o_priority = new_priority;
	}
		
  
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  struct thread * cur_thread = thread_current();
	cur_thread->nice = nice;
	int pri = sub_int_fixed(PRI_MAX, add_int_fixed(nice*2, div_fixed_int(nice, 4)));
	cur_thread -> priority = pri;
	
	/* Not yet implemented. */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  
	/* Not yet implemented. */
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
	//timer_print_stats();
	return round_convert_to_int(mul_int_fixed(100, load_avg));
  /* Not yet implemented. */
  
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  /* Not yet implemented. */
  return round_convert_to_int(thread_current()->recent_cpu*100);
}

void
thread_update_load(void)
{
	int num_thread = list_size(&ready_list);
	
	if(thread_current()!=idle_thread)
		num_thread += 1;
	
	load_avg = div_fixed_int(add_int_fixed(num_thread, mul_int_fixed(59, load_avg)),60);
	
	struct list_elem * temp = list_begin(&LIST);
	struct list_elem * end = list_end(&LIST);
	while(temp != end)
	{
		struct thread * temp_th =  list_entry(temp, struct thread, ELEM);
		int l = load_avg*2;
		temp_th->recent_cpu = ADD(MUL(DIV(l,ADD(l,1)),temp_th->recent_cpu),temp_th->nice);
	//	cpu = add_int_fixed(thread_get_nice(),mul_fixed_fixed(div_fixed_fixed(2*load_avg, add_int_fixed(1, 2*load_avg)), cpu));
		//temp_th->recent_cpu = cpu;
		temp = list_next(temp);
	}
	

}

void
thread_update_priority(void)
{
	struct list_elem * temp = list_begin(&LIST);
	struct list_elem * end = list_end(&LIST);
	
	while(temp != end)
	{
		struct thread * temp_th = list_entry(temp, struct thread, ELEM);

	
		int pri = sub_int_fixed(PRI_MAX, add_int_fixed(temp_th->nice*2 ,div_fixed_int(temp_th->recent_cpu, 4)));
		pri = round_convert_to_int(pri);
		//printf("name: %s, cpu: %d, load: %d, pri: %d\n",temp_th->name, temp_th->recent_cpu,load_avg, pri);
		if(pri>PRI_MAX)
			pri = PRI_MAX;
		if(pri<PRI_MIN)
			pri = PRI_MIN;
		temp_th->priority = pri;
		temp = list_next(temp);
	}
	list_sort(&ready_list, compare_priority, NULL);
	//printf("max %d\n",max);
	
}


/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);
                    
  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Since `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);
	//printf("name : %s\n",name);
  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  
	t->priority = priority;
	
	
	list_push_back(&LIST, &t->ELEM);
	
	t->o_priority = -1;
	
#ifdef USERPROG
	sema_init(&t->sema_wait,0);	
	sema_init(&t->sema_code,0);
	sema_init(&t->sema_load,0);
#endif

	if(thread_mlfqs)
	{

		if(t==initial_thread)
		{	
			t->recent_cpu=0;
			t->nice = 0;
		}
		else
		{
			t->recent_cpu = thread_current()->recent_cpu;
			t->nice = thread_current()->nice;
		}
	}
	list_init(&t->lock_list);
	t->magic = THREAD_MAGIC;
	t->sleep_tick = 0;
#ifdef USERPROG
	sema_init(&t->sema_wait, 0);
	t->died = false;
	t->next_fd = 4;
	t->waited = false;
	int i;
	for(i = 0; i< 131; i++)
		t->file_list[i] = NULL;
#endif
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev) 
{
  struct thread *curr = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  curr->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != curr);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   
   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void) 
{
  struct thread *curr = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (curr->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (curr != next)
    prev = switch_threads (curr, next);
  schedule_tail (prev); 
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;
  lock_acquire (&tid_lock);
	tid = next_tid++;
  lock_release (&tid_lock);
  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
int32_t thread_stack_ofs = offsetof (struct thread, stack);

void
thread_sleep (int64_t ticks)
{
	enum intr_level old_intr_level;
	struct thread * current_thread = thread_current();

//interrupt disabling to aviod data race	
		
	old_intr_level = intr_disable(); 
	if(current_thread != idle_thread && ticks>0)
	{
					
		current_thread->sleep_tick = ticks;
																
		list_push_back(&sleep_list, &current_thread -> sleep_elem);
			 
		thread_block();
	}

	intr_set_level (old_intr_level); //get interrupt state back

}

int
thread_alarm(void)
{
	struct list_elem * temp;
	struct list_elem * end;
	temp = list_begin(&sleep_list);
	end = list_end(&sleep_list);
	int waker = 0;  	
	while(temp != end)
	{
		struct thread * temp_thread = list_entry(temp, struct thread, sleep_elem);
		
		
		if(temp_thread->sleep_tick<=timer_ticks()) //whether thread wake or not
		{
		  			
						
			thread_unblock(temp_thread);
			temp = list_remove(temp);

		}
		else
		{	
			temp = list_next(temp);
		}
	}
	return waker;
}

bool 
compare_priority(struct list_elem *a, struct list_elem *b, void * aux UNUSED)
{
	struct thread * a_thread = list_entry(a, struct thread, elem);
	struct thread * b_thread = list_entry(b, struct thread, elem);
	ASSERT(is_thread(a_thread) && is_thread(b_thread));
	return a_thread->priority > b_thread->priority;
}
#ifdef USERPROG
int
wait_thread_tid(tid_t t)
{
	struct thread *cur_t = thread_current();
	struct list_elem *temp, *end;
	temp = list_begin(&LIST);
	end = list_end(&LIST);
	//printf("threa: %s\n",thread_name(), t);
	if(t == TID_ERROR)
		return -1;
	
	while(temp!=end)
	{	
		struct thread * temp_t = list_entry(temp, struct thread, ELEM);
		
		if(temp_t == NULL)
			return -1;
		if(temp_t->tid == t)
		{	
			int a;
			sema_down(&temp_t->sema_wait);
			a = temp_t->exit_code;
			//printf("thread %s: %d\n",thread_name(),a);
			return a;

		}
		temp = list_next(temp);
	}
	return -1;
}

int
code_thread_tid(tid_t t)
{
	struct thread *cur_t = thread_current();
	struct list_elem *temp, *end;
	temp = list_begin(&LIST);
	end = list_end(&LIST);
	//printf("threa2: %s\n",thread_name(), t);
	if(t == TID_ERROR)
		return -1;
	
	while(temp!=end)
	{	
		struct thread * temp_t = list_entry(temp, struct thread, ELEM);
		
		if(temp_t == NULL)
			return -1;
		if(temp_t->tid == t)
		{
			if(temp_t->parent == cur_t->tid)
			{
				int a;
				a = temp_t->exit_code;
				sema_up(&temp_t->sema_code);
				return a;
			}
			else{
				return -1;
			}
		}
		temp = list_next(temp);
	}
	//printf("end2\n");	
	return -1;
}
void
load_thread_tid(tid_t t)
{
	struct thread *cur_t = thread_current();
	struct list_elem *temp, *end;
	temp = list_begin(&LIST);
	end = list_end(&LIST);
	//printf("threa2: %s\n",thread_name(), t);
	if(t == TID_ERROR)
		return -1;
	
	while(temp!=end)
	{	
		struct thread * temp_t = list_entry(temp, struct thread, ELEM);
		
		if(temp_t == NULL)
			return -1;
		if(temp_t->tid == t)
		{	
			temp_t->parent = cur_t->tid;	
			sema_down(&temp_t->sema_load);
			return;

		}
		temp = list_next(temp);
	}
	return -1;
}
int
get_thread_tid(tid_t t)
{
	struct thread *cur_t = thread_current();
	struct list_elem *temp, *end;
	temp = list_begin(&LIST);
	end = list_end(&LIST);
	//printf("threa2: %s\n",thread_name(), t);
	if(t == TID_ERROR)
		return -1;
	
	while(temp!=end)
	{	
		struct thread * temp_t = list_entry(temp, struct thread, ELEM);
		
		if(temp_t == NULL)
			return -1;
		if(temp_t->tid == t)
		{	
			//printf("exit: %d\n",temp_t->exit_code);	
			return temp_t->exit_code;

		}
		temp = list_next(temp);
	}
	return -1;
}
void
wait_exit_thread(void)
{
	struct thread *cur_t = thread_current();
	struct list_elem *temp, *end;
	temp = list_begin(&LIST);
	end = list_end(&LIST);
	
	while(temp!=end)
	{	
		struct thread * temp_t = list_entry(temp, struct thread, ELEM);
		temp = list_next(temp);
		if(temp_t == NULL)
			return -1;
		if(temp_t->exit_code == 0)
			process_wait(temp_t->tid);
	}
}
#endif
