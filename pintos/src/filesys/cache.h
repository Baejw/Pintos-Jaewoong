#include "filesys/filesys.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include <debug.h>
#include <list.h>

void cache_init(void);
void cache_close(void);
void cache_write(struct disk *d, disk_sector_t sec_no, const void *buffer);
void cache_read(struct disk *d, disk_sector_t sec_no, void *buffer);
