#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include "threads/synch.h"
#include "devices/block.h"
#include "filesys/off_t.h"

struct buffer_head {
  /* reference 453 page of pdf */
  bool dirty;                     // 해당 entry가 dirty 인지 나타내는 flag
  bool used;                      // 해당 entry의 사용 여부를 나타내는 flag
  block_sector_t sector;          // 해당 entry의 disk sector 주소
  struct inode* inode;
  void *data;                     // buffer cache entry를 가리키기 위한 데이터 포인터
  struct lock buffer_head_lock;   // lock 변수 (struct lock)
  bool clock_bit;                 // for clock algorithm
};

bool bc_read (block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs);
bool bc_write (block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunk_size, int sector_ofs);
void bc_flush_inode_entries (struct inode *inode);
void bc_term ();
void bc_flush_all_entries ();

#endif /* filesys/buffer_cache.h */
