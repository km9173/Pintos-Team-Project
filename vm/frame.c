#include "frame.h"
#include <string.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"

#include "lib/stdio.h"

static struct list lru_list;
static struct lock lru_list_lock;
static struct list_elem *lru_clock;

void lru_list_init (void);
void add_page_to_lru_list (struct page* page);
void del_page_from_lru_list (struct page* page);
static struct list_elem *get_next_lru_clock ();
struct page *alloc_page (enum palloc_flags flags);
void free_page (void *addr);
void __free_page (struct page* page);
void *try_to_free_pages (enum palloc_flags flags);

void
lru_list_init (void)
{
  list_init (&lru_list);
  lock_init (&lru_list_lock);
  lru_clock = NULL;
}

void
add_page_to_lru_list (struct page* page)
{
  // lock_acquire (&lru_list_lock);
  list_push_back (&lru_list, &page->lru_elem);
  // lock_release (&lru_list_lock);
}

void
del_page_from_lru_list (struct page* page)
{
  // lock_acquire (&lru_list_lock);
  if (lru_clock == &page->lru_elem)
    lru_clock = get_next_lru_clock ();
  list_remove (&page->lru_elem);
  // lock_release (&lru_list_lock);
}

static struct list_elem *
get_next_lru_clock ()
{
  // lock_acquire (&lru_list_lock);
  if (lru_clock == NULL || lru_clock == list_rbegin (&lru_list))
    lru_clock = list_begin (&lru_list);
  else
    lru_clock = list_next (lru_clock);
  // lock_release (&lru_list_lock);
  return lru_clock;
}

struct page *
alloc_page (enum palloc_flags flags)
{
  void *kpage = NULL;
  struct page *new_page = NULL;

  lock_acquire (&lru_list_lock);
  kpage = palloc_get_page (flags);

  while (kpage == NULL)
  {
    kpage = try_to_free_pages (flags);
    if (kpage != NULL)
      kpage = palloc_get_page (flags);
  }

  new_page = (struct page *)malloc(sizeof(struct page));

  if (new_page == NULL)
  {
    palloc_free_page (kpage);
    return NULL;
  }
  memset (kpage, 0, 4096);
  new_page->kaddr = kpage;
  new_page->vme = NULL;
  new_page->thread = thread_current ();
  add_page_to_lru_list (new_page);
  lock_release (&lru_list_lock);

  return new_page;
}

void
free_page (void *addr)
{
  struct list_elem *it;
  lock_acquire (&lru_list_lock);
  for (it = list_begin (&lru_list); it != list_end (&lru_list); it = list_next (it)) {
    struct page *p = list_entry (it, struct page, lru_elem);
    if (addr == p->kaddr) {
      __free_page (p);
      break;
    }
  }
  lock_release (&lru_list_lock);
}

void
__free_page (struct page* page)
{
  // list_remove (&page->lru_elem);
  del_page_from_lru_list (page);
  palloc_free_page (page->kaddr);
  free (page);
}

void *
try_to_free_pages (enum palloc_flags flags UNUSED)
{
  struct list_elem *elem = NULL;
  void *kaddr = NULL;

  for (elem = get_next_lru_clock (); true; elem = get_next_lru_clock ())
  {
    struct page *p = list_entry (elem, struct page, lru_elem);
    //printf("try%x, %x", p, p->vme);
    if (p != NULL && p->vme != NULL && p->vme->is_loaded && (void *) 0x08048000 <= p->vme->vaddr && p->vme->vaddr < (void *) 0xc0000000 && pg_ofs (p->vme->vaddr) == 0)
    {
      if (pagedir_is_accessed(p->thread->pagedir, p->vme->vaddr)) {
        pagedir_set_accessed (p->thread->pagedir, p->vme->vaddr, false);
      }
      else
      {
        kaddr = p->kaddr;
        if (p->vme->type == VM_BIN)
        {
          if (pagedir_is_dirty(p->thread->pagedir, p->vme->vaddr))
          {
            // printf("[swap_out] vme->vaddr : %p\n", p->vme->vaddr);
            p->vme->swap_slot = swap_out (p->kaddr); // TODO : return 값으로 무엇을 할지 고민
            p->vme->type = VM_ANON;
          }
        }

        else if (p->vme->type == VM_FILE)
        {
          if (p->vme->is_loaded && pagedir_is_dirty(p->thread->pagedir, p->vme->vaddr))
          {
            lock_acquire(&filesys_lock);
            file_write_at(p->vme->file, p->kaddr, p->vme->read_bytes, p->vme->offset);
            lock_release(&filesys_lock);
          }
        }

        else if (p->vme->type == VM_ANON)
        {
          // printf("[swap_out] vme->vaddr : %p\n", p->vme->vaddr);
          p->vme->swap_slot = swap_out (p->kaddr); // TODO : return 값으로 무엇을 할지 고민
          //printf("A-");
        }

        else
        {
          // expect not reach here
          // if reach here then must be error case
          // printf("p->kaddr : %p\n", p->kaddr);
          // printf("p->vme->vaddr : %p\n", p->vme->vaddr);
          return NULL;
        }
        p->vme->is_loaded = false;
        pagedir_clear_page (p->thread->pagedir, p->vme->vaddr);
        __free_page (p);
        return kaddr;
      }
    }
    //else
      // printf("err ");// something error
  }
  printf("failed.. ");
  return NULL;
}
