#include "swap.h"
#include <bitmap.h>

#define BLOCK_SIZE 512
#define SLOT_SIZE 4096

static struct bitmap *swap_bitmap;

void
swap_init (size_t used_index, void* kaddr)
{

}

void
swap_in (size_t used_index, void* kaddr)
{
  
}

size_t
swap_out (void* kaddr)
{

}
