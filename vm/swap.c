#include "swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/frame.h"

#define BLOCK_SIZE 512
#define SLOT_SIZE 4096

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
  int i = 0;
  lock_acquire (&swap_lock);
  if (bitmap_test (swap_slot_bitmap, used_index))
  {
    bitmap_flip (swap_slot_bitmap, used_index);
    for (i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
    {
      block_read (swap_block, used_index * (PGSIZE / BLOCK_SECTOR_SIZE) + i, kaddr + i * BLOCK_SECTOR_SIZE);
    }
  }
  lock_release (&swap_lock);
}

size_t
swap_out (void* kaddr)
{

}
