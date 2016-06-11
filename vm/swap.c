#include "swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

static struct bitmap *swap_slot_bitmap;
static struct block *swap_block;
static struct lock swap_lock;

void
swap_init () // size_t used_index, void* kaddr
{
  lru_list_init ();
  swap_block = block_get_role (BLOCK_SWAP);
  swap_slot_bitmap =
    bitmap_create (BLOCK_SECTOR_SIZE * block_size (swap_block) / PGSIZE);
  bitmap_set_all (swap_slot_bitmap, false);
  lock_init (&swap_lock);
}

void
swap_in (size_t used_index, void* kaddr)
{
	//printf("SWAP_IN\n");
  int i = 0;
  bool flag;
  lock_acquire (&swap_lock);
//  printf("1\n");
  flag = bitmap_test (swap_slot_bitmap, used_index);
//  printf("2\n");
  // printf("[swap_in] kaddr addr : %p\n", kaddr);
  if(flag)
  {
    // printf("[swap_in] used_index : %d\n", used_index);
    bitmap_flip (swap_slot_bitmap, used_index);
    for (i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
    {
      // lock_acquire (&filesys_lock);
      block_read (swap_block, used_index * (PGSIZE / BLOCK_SECTOR_SIZE) + i, kaddr + i * BLOCK_SECTOR_SIZE);
      // lock_release (&filesys_lock);
    }
  }
  //printf("2\n");
  lock_release (&swap_lock);
}

size_t
swap_out (void* kaddr)
{
  int i = 0;
  size_t out = BITMAP_ERROR;
  lock_acquire (&swap_lock);
  out = bitmap_scan_and_flip (swap_slot_bitmap, 0, 1, false);

  if (out == BITMAP_ERROR)
  {
    lock_release (&swap_lock);
    return -1; // I'm not sure
  }

  // bitmap_flip (swap_slot_bitmap, out);
  for (i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
  {
    // lock_acquire (&filesys_lock);
    block_write (swap_block, out * (PGSIZE / BLOCK_SECTOR_SIZE) + i, kaddr + i * BLOCK_SECTOR_SIZE);
    // lock_release (&filesys_lock);
  }
  // printf("[swap_out] kaddr addr : %p\n", kaddr);
  // printf("[swap_out] out index : %p\n", out);
  lock_release (&swap_lock);
  return out;
}
