#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"

void lru_list_init (void);
struct page *alloc_page (enum palloc_flags flags);
void free_page (void *addr);

#endif /* vm/frame.h */
