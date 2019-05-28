#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT 124
#define B_SIZE 128
/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    disk_sector_t direct[DIRECT];
		disk_sector_t ddirect;
		disk_sector_t start;                /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    //uint32_t unused[125];               /* Not used. */
  };

struct indexer
{
	disk_sector_t blocks[B_SIZE];
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
get_sector(const struct inode_disk *id, off_t pos)
{
	size_t idx = pos / DISK_SECTOR_SIZE;
	disk_sector_t sector;	
	if(idx > DIRECT+B_SIZE*B_SIZE)
		PANIC("file is larger than 8MB");
	if(idx < DIRECT)
	{
		return id->direct[idx];
	}
	else
	{
		struct indexer *block = calloc(1, sizeof(struct indexer));
		size_t idx_1 = (idx-DIRECT)/B_SIZE;
		size_t idx_2 = (idx-DIRECT)%B_SIZE;
		disk_read(filesys_disk, id->ddirect, block);
		disk_read(filesys_disk, block->blocks[idx_1], block);
		sector = block->blocks[idx_2];
		free(block);
	}


	return sector;
}

static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
	if (pos < inode->data.length)
    //return get_sector(&inode->data, pos);
		return inode->data.start + pos / DISK_SECTOR_SIZE;
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */

static bool
inode_allocate(disk_sector_t sectors, off_t length)
{
	struct inode_disk *disk_inode = NULL;
	bool success = false;
	
	ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);


  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
		}
	
	size_t sector = bytes_to_sectors(length);
	int i, j;
	static char zeros[B_SIZE];
	if(sector <= DIRECT)
	{
		for(i=0;i<sector;i++)
		{
			free_map_allocate(1, &disk_inode->direct[i]);
			disk_write(filesys_disk, disk_inode->direct[i], zeros);
		}
		disk_write(filesys_disk, sectors, disk_inode);
	}
	else
	{
		
		for(i=0;i<DIRECT;i++)
		{
			free_map_allocate(1, &disk_inode->direct[i]);
			disk_write(filesys_disk, disk_inode->direct[i], zeros);
		}
		sector -= DIRECT;
		struct indexer *idx_1, *idx_2;
		idx_1 = calloc(1, sizeof(struct indexer));
		idx_2 = calloc(1, sizeof(struct indexer));
		free_map_allocate(1, &disk_inode->ddirect);
		disk_write(filesys_disk, disk_inode->ddirect, zeros);
		disk_write(filesys_disk, sectors, disk_inode);
		disk_read(filesys_disk, disk_inode->ddirect, idx_1);
		
		size_t sec_1 = DIV_ROUND_UP(sector, B_SIZE);
		for(i=0;i<sec_1;i++)
		{
			free_map_allocate(1, &idx_1->blocks[i]);
			disk_write(filesys_disk, idx_1->blocks[i],zeros);
		}
		disk_write(filesys_disk, disk_inode->ddirect, idx_1);
		
		for(i=0;i<sec_1;i++)
		{
			disk_read(filesys_disk, idx_1->blocks[i], idx_2);
			int sec_2 = sector>B_SIZE ? B_SIZE : sector;
			for(j=0;j<sec_2;j++)
			{
				free_map_allocate(1, &idx_2->blocks[j]);
				disk_write(filesys_disk, idx_2->blocks[j], zeros);
			}
			disk_write(filesys_disk, idx_1->blocks[i], idx_2);
		}
		
		free(idx_1);
		free(idx_2);
		free(disk_inode);
	}
	return true;

}
	
static bool
inode_release(struct inode *inode)
{
	off_t len = inode->data.length;
	size_t sector = bytes_to_sectors(len);
	struct inode_disk *disk_inode = &inode->data;
	struct indexer *idx_1, *idx_2;
	int i, j;
	if(sector <= DIRECT)
	{
		for(i=0;i<sector;i++)
		{
			free_map_release(disk_inode->direct[i], 1);
		}
	}
	else
	{
		for(i=0;i<DIRECT;i++)
		{
			free_map_release(disk_inode->direct[i],1);
		}

		idx_1 = calloc(1, sizeof(struct indexer));
		idx_2 = calloc(1, sizeof(struct indexer));
		disk_read(filesys_disk, disk_inode->ddirect, idx_1);
		int sec_1 = DIV_ROUND_UP(sector, B_SIZE);
		for(i=0;i<sec_1;i++)
		{	
			disk_read(filesys_disk, idx_1->blocks[i], idx_2);
			int sec_2 = sector > B_SIZE ? B_SIZE : sector;
			for(j=0;j<sec_2;j++)
			{
				free_map_release(idx_2->blocks[j], 1);
			}
			sector -= B_SIZE;
		}
	free(idx_1);
	free(idx_2);
	}
	
}

bool
inode_create (disk_sector_t sector, off_t length)
{
	bool a= false;
	if(a)
	{
		return inode_allocate(sector, length);
	}
	else
	{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start))
        {
          disk_write (filesys_disk, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[DISK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                disk_write (filesys_disk, disk_inode->start + i, zeros); 
            }
          success = true; 
        } 
      free (disk_inode);
		}
	
  	return success;
	}
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  disk_read (filesys_disk, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
        	//inode_release(inode);
					free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Read full sector directly into caller's buffer. */
          disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          disk_read (filesys_disk, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */

static bool
inode_grow(struct inode *inode, off_t length)
{
	struct inode_disk * disk_inode = &inode->data;
	off_t extended = length - inode->data.length;
	size_t e_sector = bytes_to_sectors(extended);
	size_t o_sector = bytes_to_sectors(inode->data.length);
	size_t sector = e_sector+o_sector;
	int i, j;
	static char zeros[B_SIZE];
	if(sector <= DIRECT)
	{
		for(i=o_sector;i<sector;i++)
		{
			free_map_allocate(1, &disk_inode->direct[i]);
			disk_write(filesys_disk, disk_inode->direct[i], zeros);
		}
		disk_write(filesys_disk, inode->sector, disk_inode);
	}
	else if(o_sector<DIRECT)
	{	
		for(i=o_sector;i<DIRECT;i++)
		{
			
			free_map_allocate(1, &disk_inode->direct[i]);
			disk_write(filesys_disk, disk_inode->direct[i], zeros);
		}
		sector = e_sector - (DIRECT-o_sector);
		struct indexer *idx_1, *idx_2;
		idx_1 = calloc(1, sizeof(struct indexer));
		idx_2 = calloc(1, sizeof(struct indexer));
		free_map_allocate(1, &disk_inode->ddirect);
		disk_write(filesys_disk, disk_inode->ddirect, zeros);
		disk_write(filesys_disk, inode->sector, disk_inode);
		disk_read(filesys_disk, disk_inode->ddirect, idx_1);
		
		size_t sec_1 = DIV_ROUND_UP(sector, B_SIZE);
		for(i=0;i<sec_1;i++)
		{
			free_map_allocate(1, &idx_1->blocks[i]);
			disk_write(filesys_disk, idx_1->blocks[i],zeros);
		}
		disk_write(filesys_disk, disk_inode->ddirect, idx_1);
		
		for(i=0;i<sec_1;i++)
		{
			disk_read(filesys_disk, idx_1->blocks[i], idx_2);
			size_t sec_2 = sector>B_SIZE ? B_SIZE : sector;
			for(j=0;j<sec_2;j++)
			{
				free_map_allocate(1, &idx_2->blocks[j]);
				disk_write(filesys_disk, idx_2->blocks[j], zeros);
			}
			sector -= sec_2;
			disk_write(filesys_disk, idx_1->blocks[i], idx_2);
		}
		
		free(idx_1);
		free(idx_2);
	}
	else
	{
		ASSERT(o_sector>=DIRECT);
		size_t osec_1 = DIV_ROUND_UP(o_sector-DIRECT, B_SIZE);
		size_t osec_2 = (o_sector-DIRECT)%B_SIZE;
		struct indexer *idx_1, *idx_2;
		idx_1 = calloc(1, sizeof(struct indexer));
		idx_2 = calloc(1, sizeof(struct indexer));
		disk_read(filesys_disk, disk_inode->ddirect, idx_1);
		disk_read(filesys_disk, idx_1->blocks[osec_1-1], idx_2);
		//fill osec_1 sector first
		size_t temp = e_sector > (B_SIZE-osec_2) ? B_SIZE : e_sector+osec_2;
		for(i=osec_2;i<temp;i++)
		{
			free_map_allocate(1, &idx_2->blocks[i]);
			disk_write(filesys_disk, idx_2->blocks[i], zeros);
		}
		disk_write(filesys_disk, idx_1->blocks[osec_1-1], idx_2);
		e_sector -= (temp-osec_2);
		size_t sec_1 = DIV_ROUND_UP(e_sector, B_SIZE);
		
		for(i=osec_1;i<sec_1;i++)
		{
			disk_read(filesys_disk, idx_1->blocks[i], idx_2);
			size_t sec_2 = e_sector>B_SIZE ? B_SIZE : e_sector;
			for(j=0;j<sec_2;j++)
			{
				free_map_allocate(1, &idx_2->blocks[j]);
				disk_write(filesys_disk, idx_2->blocks[j], zeros);
			}
			e_sector -= sec_2;
			disk_write(filesys_disk, idx_1->blocks[i], idx_2);
		}
		
		free(idx_1);
		free(idx_2);


	}
	free(disk_inode);
}
	
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;
	if(inode->data.length <offset + size)
	{
		inode_grow(inode, offset + size);
		inode->data.length = offset + size;
		disk_write(filesys_disk, inode->sector, &inode->data);
	}

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
			int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            disk_read (filesys_disk, sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          disk_write (filesys_disk, sector_idx, bounce); 
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
