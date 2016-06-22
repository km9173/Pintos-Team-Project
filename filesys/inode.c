#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/buffer_cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* 16 Extensible File */
#define DIRECT_BLOCK_ENTRIES 124
#define INDIRECT_BLOCK_ENTRIES 128

enum direct_t
{
  NORMAL_DIRECT,    // inode에 디스크 블록 번호를 저장
  INDIRECT,         // 1개의 인덱스 블록을 통해 디스크 블록 번호에 접근
  DOUBLE_INDIRECT,  // 2개의 인덱스 블록을 통해 디스크 블록 번호에 접근
  OUT_LIMIT         // 잘못된 파일 오프셋 값일 경우
}

struct sector_location
{
  direct_t directness;  // 디스크 블록 접근 방법(Direct, Indirect, or Double indirect)
  off_t index1;         // 첫 번째 index block에서 접근할 entry의 offset
  off_t index2;         // 두 번째 index block에서 접근할 entry의 offset
}

struct inode_indirect_block
{
  block_sector_t map_table[INDIRECT_BLOCK_ENTRIES];
}

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
// struct inode_disk size is 512 byte
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    block_sector_t direct_map_table[DIRECT_BLOCK_ENTRIES];
    block_sector_t indirect_block_sec;
    block_sector_t double_indirect_block_sec;
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock extended_lock;          /* Semaphore lock */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
// static block_sector_t
// byte_to_sector (const struct inode *inode, off_t pos)
// {
//   ASSERT (inode != NULL);
//   if (pos < inode->data.length)
//     return inode->data.start + pos / BLOCK_SECTOR_SIZE;
//   else
//     return -1;
// }

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
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start))
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0)
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;

              for (i = 0; i < sectors; i++)
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true;
        }
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
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
  block_read (fs_device, inode->sector, &inode->data);
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
block_sector_t
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
      // Project 15
      bc_flush_inode_entries (inode);

      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          free_map_release (inode->sector, 1);
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
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      bc_read (sector_idx, buffer, bytes_read, chunk_size, sector_ofs);

      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Read full sector directly into caller's buffer. */
      //     block_read (fs_device, sector_idx, buffer + bytes_read);
      //   }
      // else
      //   {
      //     /* Read sector into bounce buffer, then partially copy
      //        into caller's buffer. */
      //     if (bounce == NULL)
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //     block_read (fs_device, sector_idx, bounce);
      //     memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      //   }

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
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      bc_write (sector_idx, buffer, bytes_written, chunk_size, sector_ofs);

      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Write full sector directly to disk. */
      //     // Project 15
      //     block_write (fs_device, sector_idx, buffer + bytes_written);
      //   }
      // else
      //   {
      //     /* We need a bounce buffer. */
      //     if (bounce == NULL)
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //
      //     /* If the sector contains data before or after the chunk
      //        we're writing, then we need to read in the sector
      //        first.  Otherwise we start with a sector of all zeros. */
      //     if (sector_ofs > 0 || chunk_size < sector_left)
      //       block_read (fs_device, sector_idx, bounce);
      //     else
      //       memset (bounce, 0, BLOCK_SECTOR_SIZE);
      //     memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      //     block_write (fs_device, sector_idx, bounce);
      //   }

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

static bool
get_disk_inode (const struct inode *inode, struct inode_disk *inode_disk)
{
  /*
    inode->sector에 해당하는 on-disk inode를 buffer cache에서
    읽어 inode_disk에 저장 (bc_read() 함수 사용)
  */
  /* true 반환 */

  return bc_read (inode->sector, (void *)inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
}

static void
locate_byte (off_t pos, struct sector_location *sec_loc)
{
  off_t pos_sector = pos / BLOCK_SECTOR_SIZE;

  /* Direct 방식일 경우 */
  if (pos_sector < DIRECT_BLOCK_ENTRIES)
  {
    // sec_loc 자료구조의 변수 값 업데이트
    sec_loc->directness = NORMAL_DIRECT;
    sec_loc->index1 = pos_sector;
  }

  /* Indirect 방식일 경우 */
  else if (pos_sector < (off_t) (DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES))
  {
    // sec_loc 자료구조의 변수 값 업데이트 (구현)
    sec_loc->directness = INDIRECT;
    sec_loc->index1 = pos_sector - DIRECT_BLOCK_ENTRIES;
  }

  /* Double Indirect 방식일 경우 */
  else if (pos_sector < (off_t) (DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES * (INDIRECT_BLOCK_ENTRIES + 1)))
  {
    // sec_loc 자료구조의 변수 값 업데이트 (구현)
    sec_loc->directness = DOUBLE_INDIRECT;
    sec_loc->index1 = (pos_sector - DIRECT_BLOCK_ENTRIES - INDIRECT_BLOCK_ENTRIES) / INDIRECT_BLOCK_ENTRIES;
    sec_loc->index2 = (pos_sector - DIRECT_BLOCK_ENTRIES - INDIRECT_BLOCK_ENTRIES) % INDIRECT_BLOCK_ENTRIES;
  }

  else
    sec_loc->directness = OUT_LIMIT;
}

static inline off_t
map_table_offset (int index)
{
  /* byte 단위로 변환한 오프셋 값 return */
  return index * 4;
}

static bool
register_sector (struct inode_disk *inode_disk, block_sector_t new_sector, struct sector_location sec_loc)
{
  switch (sec_loc.directness) {
    case NORMAL_DIRECT:
      /* inode_disk에 새로 할당받은 디스크 번호 업데이트 */
      break;

    case INDIRECT:
      new_block = malloc (BLOCK_SECTOR_SIZE);
      if (new_block == NULL)
        return false;
      /* 인덱스 블록에 새로 할당 받은 블록 번호 저장 */
      /* 인덱스 블록을 buffer cache에 기록 */
      break;

    case DOUBLE_INDIRECT:
      new_block = malloc (BLOCK_SECTOR_SIZE);
      if (new_block == NULL)
        return false;
      /* 2차 인덱스 블록에 새로 할당 받은 블록 주소 저장 후,
         각 인덱스 블록을 buffer cache에 기록 */

      break;

    default:
      return false;
  }
  free (new_block);
  return true;
}

static block_sector_t
byte_to_sector (const struct inode_disk *inode_disk, off_t pos)
{
  block_sector_t result_sec; // 반환할 디스크 블록 번호

  if (pos < inode_disk->length)
  {
    struct inode_indirect_block *ind_block;
    struct sector_location sec_loc;
    locate_byte (pos, &sec_loc); // 인덱스 블록 offset 계산

    switch (sec_loc.directness)
    {
      case NORMAL_DIRECT:
        /* on-disk inode의 direct_map_table에서 디스크 블록 번호를 얻음 */
        break;

      case INDIRECT:
        ind_block = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);

        if (ind_block)
        {
          /* buffer cache에서인덱스블록을읽어옴*/
          /* 인덱스블록에서디스크블록번호확인*/
        }
        else
          result_sec = 0;

        free (ind_block);
        break;

      case DOUBLE_INDIRECT:
        ind_block = (struct inode_indirect_block *)malloc(BLOCK_SECTOR_SIZE);
        if (ind_block)
        {
          /* 1차인덱스블록을buffer cache에서읽음*/
          /* 2차인덱스블록을buffer cache에서읽음*/
          /* 2차인덱스블록에서디스크블록번호확인*/
        }
        break;
    }
  }

  return result_sec;
}

bool
inode_update_file_length (struct inode_disk* inode_disk, off_t start_pos, off_t end_pos)
{
  /* 블록 단위로 loop을 수행하며 새로운 디스크 블록 할당 */
  while (size > 0)
  {
    /* 디스크 블록 내 오프셋 계산 */
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
    if (sector_ofs > 0)
    {
      /* 블록 오프셋이 0 보다 클 경우, 이미 할당된 블록 */
    }

    else
    {
      /* 새로운 디스크 블록을 할당 */
      if (free_map_allocate (1, &sector_idx))
      {
        /* inode_disk에 새로 할당 받은 디스크 블록 번호 업데이트 */
      }
      else
      {
        free (zeroes);
        return false;
      }
      /* 새로운 디스크 블록을 0으로 초기화 */
      bc_write (sector_idx, zeroes, 0, BLOCK_SECTOR_SIZE, 0);
    }
    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
  }
  free (zeroes);
  return true;
}

static void
free_inode_sectors (struct inode_disk *inode_disk)
{
  int i = 0;
  /* Double indirect 방식으로할당된블록해지 */
  if (inode_disk->double_indirect_block_sec > 0)
  {
    /* 1차인덱스블록을buffer cache에서읽음*/
    i = 0;
    /* 1차 인덱스 블록을 통해 2차 인덱스 블록을 차례로 접근 */
    while (ind_block_1->map_table[i] > 0)
    {
      /* 2차인덱스블록을buffer cache에서읽음*/
      j = 0;
      /* 2차인덱스블록에저장된디스크블록번호를접근*/
      while (ind_block_2->map_table[j] > 0)
      {
        /* free_map업데이틀통해디스크블록할당해지*/
        j++;
      }
      /* 2차인덱스블록할당해지*/
      i++;
    }
    /* 1차인덱스블록할당해지*/
  }

  /* Indirect 방식으로할당된디스크블록해지*/
  if (inode_disk->indirect_block_sec > 0)
  {
    /* 인덱스블록을buffer cache에서읽음*/
    /* 인덱스블록에저장된디스크블록번호를접근*/
    while (ind_bloc->map_table[i] > 0)
    {
      /* free_map업데이트를통해디스크블록할당해지 */
      i++;
    }
  }

  /* Direct 방식으로할당된디스크블록해지*/
  while (inode_disk->direct_map_table[i] > 0)
  {
    /* free_map업데이트를통해디스크블록할당해지*/
    i++;
  }
}
