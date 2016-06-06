#include "swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
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
  bitmap_set_all (swap_slot_bitmap, 0);
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
  if(flag)
  {
    bitmap_flip (swap_slot_bitmap, used_index);
    for (i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
    {
      block_read (swap_block, used_index * (PGSIZE / BLOCK_SECTOR_SIZE) + i, kaddr + i * BLOCK_SECTOR_SIZE);
    }
  }
  //printf("2\n");
  lock_release (&swap_lock);
}

size_t
swap_out (void* kaddr)
{
	//printf("SWAP_OUT\n");
  lock_acquire (&swap_lock);
  size_t out = bitmap_scan (swap_slot_bitmap, 0, 1, false);
  if (out == BITMAP_ERROR)
  	return -1; // I'm not sure
  else {
    int i;
    bitmap_flip (swap_slot_bitmap, out);
    for (i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
      block_write (swap_block, out * (PGSIZE / BLOCK_SECTOR_SIZE) + i, kaddr + i * BLOCK_SECTOR_SIZE);
  }
  lock_release (&swap_lock);
  return out;
}
