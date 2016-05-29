#include "frame.h"
#include "vm/page.h"
#include "threads/synch.h"

static struct list lru_list;
static struct lock lru_list_lock;

void
lru_list_init (void)
{

}

void
add_page_to_lru_list (struct page* page)
{

}

void
del_page_from_lru_list (struct page* page)
{

}

static struct list_elem *
get_next_lru_clock ()
{

}

struct page *
alloc_page (enum palloc_flags flags)
{

}

void
free_page (void *addr)
{

}

void
__free_page (struct page* page)
{

}

void
try_to_free_pages (enum palloc_flags flags)
{

}
