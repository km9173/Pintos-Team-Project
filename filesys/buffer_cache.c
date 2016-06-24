#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/buffer_cache.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "devices/block.h"
#include "threads/malloc.h"

/* 64 * 512 byte = 32KByte */
#define BUFFER_CACHE_ENTRY_NB 64

// buffer cache 메모리 영역을 가리킴
static void *p_buffer_cache[BUFFER_CACHE_ENTRY_NB];

// buffer head 배열
static struct buffer_head buffer_head_table[BUFFER_CACHE_ENTRY_NB];

static struct lock buffer_cache_lock;

void bc_init (void);
void bc_term (void);
struct buffer_head* bc_select_victim (void);
struct buffer_head* bc_lookup (block_sector_t sector);
void bc_flush_entry (struct buffer_head *p_flush_entry);
void bc_flush_all_entries (void);

bool
bc_read (block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs)
{
  lock_acquire (&buffer_cache_lock);
  /* sector_idx를 buffer_head에서 검색 (bc_lookup함수이용) */
  struct buffer_head* cached = bc_lookup (sector_idx);

  if (cached != NULL)
    lock_acquire (&cached->buffer_head_lock);

  else
  {
    cached = bc_select_victim ();

    // for debugging
    if (!cached) {
      printf("bc_read : Shouldn't happen!\n");
      return false;
    }

    lock_acquire (&cached->buffer_head_lock);
    cached->dirty = false;
    cached->used = true;
    cached->sector = sector_idx;
    cached->inode = inode_open (sector_idx);
    /* block_read 함수를 이용해, 디스크 블록데이터를 buffer cache로 read */
    block_read (fs_device, sector_idx, cached->data);
  }
  /* memcpy 함수를 통해, buffer에 디스크 블록데이터를 복사 */
  memcpy (buffer + bytes_read, cached->data + sector_ofs, chunk_size);

  /* buffer_head 의 clock bit을 setting */
  cached->clock_bit = true;
  lock_release (&cached->buffer_head_lock);

  lock_release (&buffer_cache_lock);
  return true;
}

bool
bc_write (block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunk_size, int sector_ofs)
{
  lock_acquire (&buffer_cache_lock);
  struct buffer_head* cached = NULL;

  // check if entry exist
  cached = bc_lookup (sector_idx);

  if (cached != NULL)
    lock_acquire (&cached->buffer_head_lock);

  // if not exist in entry
  else
  {
    cached = bc_select_victim ();

    if (cached->data == NULL)
      return false;

    lock_acquire (&cached->buffer_head_lock);
    cached->sector = sector_idx;
    cached->used = true;
    cached->inode = inode_open (sector_idx);

    block_read (fs_device, sector_idx, cached->data);
  }

  memcpy (cached->data + sector_ofs, buffer + bytes_written, chunk_size);
  cached->dirty = true;
  cached->clock_bit = true;
  lock_release (&cached->buffer_head_lock);
  lock_release (&buffer_cache_lock);
  return true;
}

void
bc_init (void)
{
  int i;

  /* Allocation buffer cache in Memory */
  /* p_buffer_cache가 buffer cache 영역포인팅 */
  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++) {
    p_buffer_cache[i] = (void *)malloc(BLOCK_SECTOR_SIZE);
  }

  /* 전역변수buffer_head자료구조초기화*/
  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++) {
    buffer_head_table[i].dirty = false;
    buffer_head_table[i].used = false;
    buffer_head_table[i].sector = -1;
    buffer_head_table[i].inode = NULL;
    buffer_head_table[i].data = p_buffer_cache[i];
    lock_init (&buffer_head_table[i].buffer_head_lock);
    buffer_head_table[i].clock_bit = false;
  }

  lock_init (&buffer_cache_lock);
}

void
bc_term (void)
{
  lock_acquire (&buffer_cache_lock);
  int i = 0;

  bc_flush_all_entries ();

  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
  {
    lock_acquire (&buffer_head_table[i].buffer_head_lock);
    if (buffer_head_table[i].used && buffer_head_table[i].data != NULL)
      free (buffer_head_table[i].data);
    buffer_head_table[i].dirty = false;
    buffer_head_table[i].used = false;
    buffer_head_table[i].sector = 0;
    buffer_head_table[i].inode = NULL;
    buffer_head_table[i].data = NULL;
    buffer_head_table[i].clock_bit = false;
    lock_release (&buffer_head_table[i].buffer_head_lock);
  }
  lock_release (&buffer_cache_lock);
}

struct buffer_head*
bc_select_victim (void)
{
  int i;
  /* clock 알고리즘을 사용하여 victim entry를 선택*/

  /* buffer_head 전역변수를 순회하며 clock_bit 변수를 검사*/
  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
    if (!buffer_head_table[i].used)
      return &buffer_head_table[i];

  // lock이 필요한 지 의문
  i = 0;
  while (!buffer_head_table[i].clock_bit) {
    if (buffer_head_table[i].clock_bit) {
      lock_acquire (&buffer_head_table[i].buffer_head_lock);
      buffer_head_table[i].clock_bit = false;
      buffer_head_table[i].used = true;
      lock_release (&buffer_head_table[i].buffer_head_lock);
    }

    if (i >= BUFFER_CACHE_ENTRY_NB)
      i = 0;
    else
      i++;
  }

  lock_acquire (&buffer_head_table[i].buffer_head_lock);
  if (buffer_head_table[i].dirty)
    bc_flush_entry (&buffer_head_table[i]);

  /* victim entry에 해당하는 buffer_head값 update */
  buffer_head_table[i].dirty = false;
  buffer_head_table[i].sector = 0;
  buffer_head_table[i].inode = NULL;
  memset (buffer_head_table[i].data, 0, BLOCK_SECTOR_SIZE);
  buffer_head_table[i].clock_bit = false;
  lock_release (&buffer_head_table[i].buffer_head_lock);

  /* victim entry를return */
  return &buffer_head_table[i];
}

struct buffer_head*
bc_lookup (block_sector_t sector)
{
  int i = 0;

  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
  {
    lock_acquire (&buffer_head_table[i].buffer_head_lock);
    if (buffer_head_table[i].sector == sector)
    {
      lock_release (&buffer_head_table[i].buffer_head_lock);
      return &buffer_head_table[i];
    }
    lock_release (&buffer_head_table[i].buffer_head_lock);
  }
  return NULL;
}

void
bc_flush_entry (struct buffer_head *p_flush_entry)
{
  block_write (fs_device, p_flush_entry->sector, p_flush_entry->data);
  p_flush_entry->dirty = false;
}

void
bc_flush_all_entries (void)
{
  int i = 0;

  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
  {
    if (buffer_head_table[i].used && buffer_head_table[i].dirty)
    {
      lock_acquire (&buffer_head_table[i].buffer_head_lock);
      bc_flush_entry(&buffer_head_table[i]);
      lock_release (&buffer_head_table[i].buffer_head_lock);
    }
  }
}

void
bc_flush_inode_entries (struct inode *inode)
{
  int i = 0;
  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
  {
    if (buffer_head_table[i].used && buffer_head_table[i].inode == inode)
    {
      lock_acquire (&buffer_head_table[i].buffer_head_lock);
      bc_flush_entry(&buffer_head_table[i]);
      lock_release (&buffer_head_table[i].buffer_head_lock);
    }

  }
}
