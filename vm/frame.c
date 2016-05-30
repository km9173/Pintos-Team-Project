#include "frame.h"
#include <string.h>
#include "vm/page.h"
#include "threads/synch.h"

static struct list lru_list;
static struct lock lru_list_lock;
static struct list_elem lru_clock;

void
lru_list_init (void)
{
  list_init (&lru_list);
  lock_init (&lru_list_lock);
  // TODO : lru_clock의 값을 NULL로 설정
  // 문제는 lru_clock 에 대한 언급이 실습 자료에 없음..
  // 아무래도 lru_clock은 page를 가리키는 포인터여야 할 것 같다.
}

void
add_page_to_lru_list (struct page* page)
{

}

void
del_page_from_lru_list (struct page* page)
{
  lock_acquire (&lru_list_lock);
  list_remove (&page->lru_elem);
  lock_release (&lru_list_lock);
}

static struct list_elem *
get_next_lru_clock ()
{

}

struct page *
alloc_page (enum palloc_flags flags)
{
  uint8_t *kpage = NULL;
  struct page *new_page = NULL;

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

  new_page->kaddr = kpage;
  new_page->vme = NULL; // (struct vm_entry *)malloc(sizeof(struct vm_entry));
  new_page->thread = thread_current ();

  add_page_to_lru_list (new_page);

  return new_page;
}

void
free_page (void *addr)
{

}

void
__free_page (struct page* page)
{

}

void *
try_to_free_pages (enum palloc_flags flags)
{
  struct list_elem *elem = NULL;
  void *kaddr = NULL;

  for (elem = get_next_lru_clock (); true; elem = get_next_lru_clock ())
  {
    struct page *p = list_entry (elem, struct page, lru_elem);
    if (p != NULL && p->vme != NULL)
    {
      if (pagedir_is_accessed(p->thread->pagedir, p->vme->vaddr))
        pagedir_set_accessed (p->thread->pagedir, p->vme->vaddr, false);
      else
      {
        kaddr = p->kaddr;
        if (p->vme->type == VM_BIN)
        {
          if (pagedir_is_dirty(p->thread->pagedir, p->vme->vaddr))
            swap_out (p->kaddr); // TODO : return 값으로 무엇을 할지 고민
          p->vme->type = ANON;
        }

        else if (p->vme->type == FILE)
        {
          if (p->vme->is_loaded && pagedir_is_dirty(p->thread->pagedir, p->vme->vaddr))
          {
            lock_acquire(&filesys_lock);
            file_write_at(p->vme->file, p->vme->vaddr, p->vme->read_bytes, p->vme->offset);
            lock_release(&filesys_lock);
          }
        }

        else if (p->vme->type == ANON)
        {
          swap_out (p->kaddr); // TODO : return 값으로 무엇을 할지 고민
        }

        else
        {
          // expect not reach here
          ; // must be error
        }
        __free_page (p); // palloc_free_page (p->kaddr);
        pagedir_clear_page (p->thread->pagedir, p->vme->vaddr);
        return kaddr;
      }
    }
    else
      printf("error!\n");// something error

    return NULL;
  }
}
