#include "swap.h"
#include <bitmap.h>
#include <block.h>
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/frame.h"

#define BLOCK_SIZE 512
#define SLOT_SIZE 4096

static struct bitmap *swap_slot_bitmap;
static struct block *swap_block;
static struct lock swap_lock;

void
swap_init (size_t used_index, void* kaddr)
{
  lru_list_init ();
  swap_block = block_get_role (BLOCK_SWAP);
  swap_slot_bitmap =
    bitmap_create (BLOCK_SECTOR_SIZE * block_size(swap_block) / PGSIZE);
  bitmap_set_all (swap_slot_bitmap, 0);
  lock_init (&swap_lock);
}

void
swap_in (size_t used_index, void* kaddr)
{

}

size_t
swap_out (void* kaddr)
{

}
