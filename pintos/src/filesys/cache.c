#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include <debug.h>
#include <list.h>

#define BUFFER_SIZE 32
struct buffer_cache
{
	bool used;
	bool dirty;
	bool clock_bit;

	disk_sector_t disk_sector;
	uint8_t buffer[DISK_SECTOR_SIZE];
};

struct ahead_entry
{
	disk_sector_t disk_sector;
	struct list_elem ahead_elem;
};

static struct buffer_cache cache[BUFFER_SIZE];
static struct lock cache_lock;
static struct condition cond_ahead;
static struct lock cond_lock;
static struct list ahead_list;
static bool close_ahead = true;
static uint8_t clock;


void cache_read_ahead(void *aux);
void cache_init(void);
void cache_close(void);
void cache_write(struct disk *d, disk_sector_t sec_no, const void *buffer);
void cache_read(struct disk *d, disk_sector_t sec_no, void *buffer);
int cache_evict(struct disk *d);
void cache_flush(uint8_t evict_no,struct disk *d);

void
cache_read_ahead(void *aux UNUSED)
{
	cond_init(&cond_ahead);
	lock_init(&cond_lock);
	
	while(!close_ahead)
	{
		lock_acquire(&cond_lock);
		cond_wait(&cond_ahead, &cond_lock);
		lock_acquire(&cache_lock);
		
		while(!list_empty(&ahead_list))	
		{	
			struct list_elem *temp = list_pop_front(&ahead_list);
			struct ahead_entry *temp_ahead = list_entry(temp, struct ahead_entry, ahead_elem); 
				
			int evict_no = cache_evict(filesys_disk);
			disk_read(filesys_disk, temp_ahead->disk_sector, cache[evict_no].buffer);
			cache[evict_no].disk_sector = temp_ahead->disk_sector;
			cache[evict_no].used = true;
			cache[evict_no].clock_bit = true;

		
		}

		lock_release(&cache_lock);


		lock_release(&cond_lock);
	}

}

void
cache_ahead_close(void)
{
	close_ahead = true;
	cond_signal(&cond_ahead, &cond_lock);
}

void
cache_init(void)
{
	int i;
	lock_init(&cache_lock);
	list_init(&ahead_list);
	close_ahead = false;
	clock = 0;
	for(i=0; i<BUFFER_SIZE; i++)
	{
		cache[i].used = false;
		cache[i].clock_bit = false;
		cache[i].dirty = false;
	}
	int *aux;
	//thread_create("ahead_thread", PRI_DEFAULT, cache_read_ahead, aux);

}

void 
cache_write(struct disk *d, disk_sector_t sec_no, const void *buffer)
{
	
	lock_acquire(&cache_lock);
	disk_write(d, sec_no, buffer);
	
	int i;
	for(i=0; i<BUFFER_SIZE; i++)
	{
		if(cache[i].used, cache[i].disk_sector == sec_no)
		{
			memcpy(cache[i].buffer, buffer, DISK_SECTOR_SIZE);
			cache[i].dirty = true;
			cache[i].used = true;
			cache[i].clock_bit = true;
			goto done;
		}
	}

	int evict_no = cache_evict(d);
	disk_write(d, sec_no, buffer);
	memcpy(cache[evict_no].buffer, buffer, DISK_SECTOR_SIZE);
	cache[evict_no].disk_sector = sec_no;
	cache[evict_no].used = true;
	cache[evict_no].clock_bit = true;
	
done:
	lock_release(&cache_lock);

}

void
cache_read(struct disk *d, disk_sector_t sec_no, void *buffer)
{
	
	lock_acquire(&cache_lock);
	int i;
	struct ahead_entry *next_block=NULL;
	next_block = palloc_get_page(0);
	for(i=0; i<BUFFER_SIZE; i++)
	{
		if(cache[i].used && cache[i].disk_sector == sec_no)
		{
			memcpy(buffer, cache[i].buffer, DISK_SECTOR_SIZE);
			cache[i].clock_bit = true;
			goto done;
		}
			
	}
	
	int evict_no = cache_evict(d);
	disk_read(d, sec_no, cache[evict_no].buffer);
	memcpy(buffer, cache[evict_no].buffer, DISK_SECTOR_SIZE);
	cache[evict_no].disk_sector = sec_no;
	cache[evict_no].used = true;
	cache[evict_no].clock_bit = true;
	
done:
	next_block->disk_sector = sec_no;
	//list_push_back(&ahead_list, &next_block->ahead_elem);
	lock_release(&cache_lock);
}

void
cache_close(void)
{	
	lock_acquire(&cache_lock);
	int i;
	for(i=0; i<BUFFER_SIZE; i++)
	{
		if(cache[i].used)
			cache_flush(filesys_disk,i);
	}
	lock_release(&cache_lock);
	//cache_ahead_close();
}

int
cache_evict(struct disk *d)
{
	int evict_no;
	ASSERT(lock_held_by_current_thread(&cache_lock));
	while(true)
	{
		if(!cache[clock].used)
			return clock;

		if(cache[clock].clock_bit)
			cache[clock].clock_bit = false;
		else
		{
			evict_no = clock;
			break;
		}

		if(clock==BUFFER_SIZE-1)
			clock = 0;
		else
			clock++;
	}

	if(cache[evict_no].dirty)
		cache_flush(evict_no, d);

	return evict_no;

}

void
cache_flush(uint8_t evict_no, struct disk *d)
{
	ASSERT(lock_held_by_current_thread(&cache_lock));
	if(cache[evict_no].dirty)
		disk_write(d, cache[evict_no].disk_sector, cache[evict_no].buffer);
	cache[evict_no].dirty = false;
}

