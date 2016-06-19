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

void bc_init (void);
void bc_term (void);
struct buffer_head* bc_select_victim (void);
struct buffer_head* bc_lookup (block_sector_t sector);
void bc_flush_entry (struct buffer_head *p_flush_entry);
void bc_flush_all_entries (void);

bool
bc_read (block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs)
{
  /* sector_idx를buffer_head에서검색(bc_lookup함수이용)*/
  struct buffer_head* cache = bc_lookup (sector_idx);

  if (!cache) {
    cache = bc_select_victim ();

    // for debugging
    if (!cache) {
      printf("bc_read : Shouldn't happen!\n");
      return false;
    }
    /* block_read함수를이용해, 디스크블록데이터를buffer cache로read */
    block_read (cache->data, sector_idx, buffer);
  }
  /* memcpy함수를통해, buffer에디스크블록데이터를복사*/
  memcpy (cache->data + sector_ofs, buffer + bytes_read, chunk_size);

  /* buffer_head의clock bit을setting */
  cache->clock_bit = true;

  return true;
}

bool
bc_write (block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunk_size, int sector_ofs)
{
  int i = 0;
  // bool success = false; // TODO : 쓸지말지 고민..
  struct buffer_head* cached = NULL;

  // check if entry exist
  cached = bc_lookup (sector_idx);

  // if not exist in entry
  if (cached == NULL)
  {
    for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
    {
      if (!buffer_head_table[i].used)
      {
        buffer_head_table[i].used = true;
        cached = &buffer_head_table[i];
        break;
      }
    }

    // if cached full
    if (cached == NULL)
    {
      cached = bc_select_victim ();
      if (cached->dirty)
        bc_flush_entry (cached);
    }

    // TODO: 만약 cached->data가 NULL인 경우가 있을까?
    // if (cached->data == NULL)
    // {
    //   p_buffer_cache
    // }

    memset (cached->data, 0, BLOCK_SECTOR_SIZE);
    cached->dirty = false;
    cached->used = false;
    cached->sector = 0;
    cached->inode = NULL;
    cached->clock_bit = false;

    if (cached->data == NULL)
      return false;

    cached->sector = sector_idx;
    cached->inode = inode_open (sector_idx);

    block_read (fs_device, sector_idx, cached->data);
  }

  memcpy (cached->data + sector_ofs, buffer + bytes_written, chunk_size);

  cached->used = true;   // 매번 used true 해줘야하는기 고민할 필요가있음.. flsuh 하면 뭐뭐가 바뀌는지 알아보자.
  cached->dirty = true;

  return true;
}

void
bc_init (void)
{
  int i;

  /* Allocation buffer cache in Memory */
  /* p_buffer_cache가buffer cache 영역포인팅*/
  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++) {
    p_buffer_cache[i] = malloc(sizeof(BLOCK_SECTOR_SIZE) * BUFFER_CACHE_ENTRY_NB);
    // temp = malloc(sizeof(BLOCK_SECTOR_SIZE) * BUFFER_CACHE_ENTRY_NB);
  }

  /* 전역변수buffer_head자료구조초기화*/
  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++) {
    buffer_head_table[i].dirty = false;
    buffer_head_table[i].used = false;
    buffer_head_table[i].sector = 0;
    buffer_head_table[i].inode = NULL;
    buffer_head_table[i].data = p_buffer_cache[i];
    lock_init (&buffer_head_table[i].buffer_head_lock);
    buffer_head_table[i].clock_bit = false;
  }
}

void
bc_term (void)
{
  int i = 0;

  bc_flush_all_entries ();

  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
  {
    if (buffer_head_table[i].used && buffer_head_table[i].data != NULL)
      free (buffer_head_table[i].data);
      buffer_head_table[i].dirty = false;
      buffer_head_table[i].used = false;
      buffer_head_table[i].sector = 0;
      buffer_head_table[i].inode = NULL;
      buffer_head_table[i].data = NULL;
      buffer_head_table[i].clock_bit = false;
  }
}

struct buffer_head*
bc_select_victim (void)
{
  int i;
  /* clock 알고리즘을사용하여victim entry를선택*/

  /* buffer_head전역변수를순회하며clock_bit변수를검사*/
  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
    if (!buffer_head_table[i].used)
      return &buffer_head_table[i];

  // lock이 필요한 지 의문
  i = 0;
  while (!buffer_head_table[i].clock_bit) {
    if (buffer_head_table[i].clock_bit) {
      lock_acquire (&buffer_head_table[i].buffer_head_lock);
      buffer_head_table[i].clock_bit = false;
      lock_release (&buffer_head_table[i].buffer_head_lock);
    }
    else {
      /* 선택된victim entry가dirty일경우, 디스크로flush */
      if (buffer_head_table[i].dirty) {
        bc_flush_entry (&buffer_head_table[i]);
        break;
      }
    }

    if (i >= BUFFER_CACHE_ENTRY_NB)
      i = 0;
    else
      i++;
  }

  /* victim entry에해당하는buffer_head값update */
  lock_acquire (&buffer_head_table[i].buffer_head_lock);
  buffer_head_table[i].dirty = false;
  buffer_head_table[i].used = false;
  buffer_head_table[i].sector = 0;
  buffer_head_table[i].inode = NULL;
  buffer_head_table[i].data = NULL;
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
    if (buffer_head_table[i].sector == sector)
      return &buffer_head_table[i];
  }
  return NULL;
}

void
bc_flush_entry (struct buffer_head *p_flush_entry)
{
  lock_acquire (&p_flush_entry->buffer_head_lock);
  block_write (p_flush_entry->data, p_flush_entry->sector, p_flush_entry->data);
  p_flush_entry->dirty = false;
  lock_release (&p_flush_entry->buffer_head_lock);
}

void
bc_flush_all_entries (void)
{
  int i = 0;

  for (i = 0; i < BUFFER_CACHE_ENTRY_NB; i++)
  {
    if (buffer_head_table[i].used && buffer_head_table[i].dirty)
      bc_flush_entry(&buffer_head_table[i]);
  }
}
