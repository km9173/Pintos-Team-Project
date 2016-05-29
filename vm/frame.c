#include "frame.h"
#include "vm/page.h"

static struct list lru_list;
static struct lock lru_list_lock;

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
  list_remove (&page->lru_elem);
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
