#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include "devices/input.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "debug.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);
void get_argument(void *esp, int *arg, int count);
struct vm_entry *check_address (void *addr, void *esp UNUSED);
void check_valid_buffer (void *buffer, unsigned size, void *esp, bool to_write);
void check_valid_string (const void *str, void *esp);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  // int status, fd, pid;
  int arg[4] = {0, };
  // char *file = NULL, *cmd_line = NULL;
  // void *buffer;
  // unsigned size, position;

  // Check if stack pointer is in the user memory area
  check_address (f->esp, f->esp);

  // Save user stack arguments in kernel
  switch (*(int *)(f->esp))
  {
  // f->eax = return value;
    case SYS_HALT:
      halt ();
      break;

    case SYS_EXIT:
      get_argument (f->esp, (int *)arg, 1);
      check_address((void *)(arg[0]), f->esp);
      // status = *(int *)arg[0];
      // 0: status
      exit(*(int *)arg[0]);
      break;

    case SYS_EXEC:
      get_argument (f->esp, (int *)arg, 1);
      // check_address((void *)(arg[0]));
      check_valid_string ((const void *)arg[0], f->esp);
      // cmd_line = *(char **)arg[0];
      // 0: cmd_line
      f->eax = exec (*(const char **)arg[0]);
      break;

    case SYS_WAIT:
      get_argument (f->esp, (int *)arg, 1);
      check_address ((void *)arg[0], f->esp);
      // pid = *(int *)arg[0];
      // 0: pid
      f->eax = wait (*(int *)arg[0]);
      break;

    case SYS_CREATE:
      get_argument (f->esp, (int *)arg, 2);
      // check_address((void *)arg[0]);
      check_valid_string ((const void *)arg[0], f->esp);
      check_address((void *)arg[1], f->esp);
      // file = *(char **)arg[0];
      // size = *(int *)arg[1];
      // 0: file, 1: size
      f->eax = create(*(const char **)arg[0], *(int *)arg[1]);
      break;

    case SYS_REMOVE:
      get_argument (f->esp, (int *)arg, 1);
      // check_address((void *)arg[0]);
      check_valid_string ((const void *)arg[0], f->esp);
      // file = *(char **)arg[0];
      // 0: file
      f->eax = remove (*(const char **)arg[0]);
      break;

    case SYS_OPEN:
      get_argument (f->esp, (int *)arg, 1);
      // check_address((void *)arg[0]);
      check_valid_string ((const void *)arg[0], f->esp);
      // file = *(char **)arg[0];
      // 0: file
      f->eax = open (*(const char **)arg[0]);
      break;

    case SYS_FILESIZE:
      get_argument (f->esp, (int *)arg, 1);
      check_address((void *)arg[0], f->esp);
      // fd = *(int *)arg[0];
      // 0: fd
      f->eax = filesize (*(int *)arg[0]);
      break;

    case SYS_READ:
      get_argument (f->esp, (int *)arg, 3);
      check_address((void *)arg[0], f->esp);
      // check_address((void *)arg[1]);
      check_address((void *)arg[2], f->esp);
      check_valid_buffer (*(void **)arg[1], *(int *)arg[2], f->esp, true);
      // fd = *(int *)arg[0];
      // buffer = *(void **)arg[1];
      // size = *(int *)arg[2];
      // check_address (buffer + size);
      // 0: fd, 1: buffer, 2: size
      // check_address ((void *)(arg[1] + arg[2]), f->esp);
      f->eax = read (*(int *)arg[0], *(void **)arg[1], *(unsigned *)arg[2]);
      break;

    case SYS_WRITE:
      get_argument (f->esp, (int *)arg, 3);
      check_address((void *)arg[0], f->esp);
      // check_address((void *)arg[1]);
      check_valid_string ((void *)arg[1], f->esp);
      check_address((void *)arg[2], f->esp);
      // fd = *(int *)arg[0];
      // buffer = *(void **)arg[1];
      // size = *(int *)arg[2];
      // check_address (buffer + size);
      // 0: fd, 1: buffer, 2: size
      // check_address ((void *)(arg[1] + arg[2]), f->esp);
      f->eax = write (*(int *)arg[0], *(void **)arg[1], *(unsigned *)arg[2]);
      break;

    case SYS_SEEK:
      get_argument (f->esp, (int *)arg, 2);
      check_address((void *)arg[0], f->esp);
      check_address((void *)arg[1], f->esp);
      // fd = *(int *)arg[0];
      // position = *(unsigned *)arg[1];
      // 0: fd, 1: position
      seek (*(int *)arg[0], *(unsigned *)arg[1]);
      break;

    case SYS_TELL:
      get_argument (f->esp, (int *)arg, 1);
      check_address((void *)arg[0], f->esp);
      // fd = *(int *)arg[0];
      // 0: fd
      f->eax = tell (*(int *)arg[0]);
      break;

    case SYS_CLOSE:
      get_argument (f->esp, (int *)arg, 1);
      check_address((void *)arg[0], f->esp);
      // fd = *(int *)arg[0];
      // 0: fd
      close (*(int *)arg[0]);
      break;

    case SYS_MMAP:              /* Map a file into memory. */
      get_argument(f->esp , (int *)arg , 2);
      f -> eax = mmap (*(int *)arg[0], *(void **)arg[1]);
      break;

    case SYS_MUNMAP:            /* Remove a memory mapping. */
      get_argument(f->esp , (int *)arg , 1);
      munmap (*(int *)arg[0]);
      break;

    default:
      thread_exit ();
  }
}

void
get_argument (void *esp, int *arg, int count)
{
  int i = 0;
  for (i = 0; i < count; i++)
  {
    esp = esp + 4;
    check_address(esp, esp);  // Check if *esp address in user memory area
    arg[i] = (int)esp;
  }
}

struct vm_entry *
check_address (void *addr, void *esp UNUSED)
{
  // Check addr is user memory area, If invalid access then exit process
  if (addr < (void *) 0x8048000 || addr >= (void *) 0xc0000000)
    exit (-1);

  return find_vme (addr);
}

void
halt (void)
{
  shutdown_power_off();
}

void
exit (int status)
{
  struct thread *t = thread_current();
  t->exit_status = status;  // Store exit status in PCB
  printf("%s: exit(%d)\n", t->name, status);
  thread_exit();
}

pid_t
exec (const char *cmd_line)
{
  pid_t pid;
  struct thread *child_process;
  //struct thread *t = thread_current();

  pid = process_execute(cmd_line);
  child_process = get_child_process(pid);

  sema_down(&child_process->load);

  if (!child_process->memory_load_success)
    return -1;

  return pid;
}

int
wait (pid_t pid)
{
  return process_wait(pid);
}

bool
create (const char *file, unsigned initial_size)
{
  if (file == NULL)
    exit (-1);

  return filesys_create(file, initial_size);
}

bool
remove (const char *file)
{
  bool success = filesys_remove(file);
  return success;
}

int
open (const char *file)
{
  struct thread *t = thread_current();
  struct file *f = NULL;
  int fd = FD_MIN;

  if (file == NULL)
    return -1;

  lock_acquire (&filesys_lock);

  f = filesys_open(file);

  if (f == NULL)
  {
    lock_release (&filesys_lock);
    return -1;
  }

  while(t->fd_table[fd])
    fd++;

  t->fd_table[fd] = f;

  if (fd == t->fd_size)
    t->fd_size++;

  lock_release (&filesys_lock);
  return fd;
}

int
filesize (int fd)
{
  struct file *f = process_get_file (fd);
  if (!f)
    return -1;
  else
    return file_length (f);
}

int
read (int fd, void *buffer, unsigned size)
{
  struct file *f = NULL;
  unsigned i = 0;
  int read_size = 0;


  if (fd == STDIN_FILENO)
  {
    for(i = 0; i < size; i++)
    {
      ((char*)buffer)[i] = input_getc();
    }
    read_size = size;
  }

  else
  {
    lock_acquire(&filesys_lock);
    f = process_get_file(fd);

    if (f == NULL)
      read_size = -1;
    else
    {
      // printf("[read syscall] fd : %d\n", fd);
      // printf("[read syscall] buffer addr : %p\n", buffer);
      read_size = file_read(f, buffer, size);
    }
    lock_release(&filesys_lock);
  }

  return read_size;
}

int
write (int fd, void *buffer, unsigned size)
{
  struct file *f = NULL;
  int read_size = 0;

  if (fd == STDIN_FILENO)
		return -1;



  if (fd == STDOUT_FILENO)
  {
    putbuf ((char *)buffer, size);
    read_size = size;
  }

  else
  {
    lock_acquire (&filesys_lock);
    f = process_get_file (fd);
    if (f == NULL)
      read_size = -1;
    else
      read_size = file_write (f, buffer, size);

    lock_release (&filesys_lock);
  }

  return read_size;
}

void
seek (int fd, unsigned position)
{
  struct file *f = process_get_file (fd);

  file_seek (f, position);
}

unsigned
tell (int fd)
{
  struct file *f = process_get_file(fd);

  return file_tell (f);
}

void
close (int fd)
{
  lock_acquire (&filesys_lock);
  struct thread *t = thread_current();

  process_close_file(fd);
  t->fd_size--;
  lock_release (&filesys_lock);
}

void
check_valid_buffer (void *buffer, unsigned size, void *esp, bool to_write)
{
  struct vm_entry *vme = NULL;
  int count = 0;

  while ((int)size > count * 1024)
  {
    vme = check_address (buffer + count * 1024, esp);
    if (vme == NULL || vme->writable != to_write)
      exit (-1);
    count++;
  }
}

void
check_valid_string (const void *str, void *esp)
{
  void *string = (void *)str;
  if (check_address (string, esp) == NULL)
    exit (-1);
}

// 12. Memory mapped file Project
mapid_t
mmap (int fd, void *addr)
{
  struct thread *cur = NULL;
  struct file *old_file = NULL;
  struct file *new_file = NULL;
  struct mmap_file *mmap_f = NULL;
  off_t ofs = 0;
  uint32_t read_bytes;

  if ((old_file = process_get_file (fd)) == NULL)
    return -1;

  // check_address func을 쓰지 않는게 올바른 선택인가?
  // 테스트 케이스중에 exit(-1)을 유도하는 건지 return -1을 유도하는 건지 모르겠다.
  if (addr < (void *)0x08048000 || addr >= (void *)0xc0000000 ||
     ((uint32_t) addr % PGSIZE) != 0)
    return -1;

  lock_acquire(&filesys_lock);
  new_file = file_reopen (old_file);
  lock_release(&filesys_lock);
  if (new_file == NULL) // if we check file_length?
    return -1;

  read_bytes = file_length (new_file);

  mmap_f = (struct mmap_file *)malloc(sizeof(struct mmap_file));
  if (mmap_f == NULL)
    return -1;

  list_init (&mmap_f->vme_list);
  cur = thread_current();

  while (read_bytes > 0)
  {
    /* Calculate how to fill this page.
       We will read PAGE_READ_BYTES bytes from FILE
       and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    struct vm_entry *vme = (struct vm_entry *)malloc(sizeof(struct vm_entry));
    if (vme != NULL)
    {
      vme->type = VM_FILE;
      vme->vaddr = pg_round_down(addr);
      vme->writable = true;
      vme->is_loaded = false;
      vme->file = new_file;
      vme->offset = ofs;
      vme->read_bytes = page_read_bytes;
      vme->zero_bytes = page_zero_bytes;
      vme->swap_slot = 0;
    }
    else
      return -1;

    if (!insert_vme (&cur->vm, vme))
    {
      free(vme);
      return -1;
    }

    list_push_back(&mmap_f->vme_list , &vme->mmap_elem);

    /* Advance. */
    read_bytes -= page_read_bytes;
    ofs += page_read_bytes;
    addr += PGSIZE;
  }

  mmap_f->mapid = cur->mmap_list_count;
  cur->mmap_list_count += 1;
  mmap_f->file = new_file;
  list_push_back(&cur->mmap_list , &mmap_f->elem);

  return mmap_f->mapid;
}

void
munmap (int mapid)
{
  struct thread *cur = thread_current();
  struct list_elem *it;

  for (it = list_begin(&cur->mmap_list); it != list_end(&cur->mmap_list); )
  {
  	struct mmap_file *mmap_f = list_entry(it, struct mmap_file, elem);
  	if (mapid == CLOSE_ALL || mapid == mmap_f->mapid)
	{
      // do_munmap
      while (!list_empty(&mmap_f->vme_list))
      {
      	struct list_elem *i = list_pop_front(&mmap_f->vme_list);
      	struct vm_entry *vme = list_entry(i, struct vm_entry, mmap_elem);
      	if (vme->is_loaded && pagedir_is_dirty (cur->pagedir, vme->vaddr))
      	{
      	  lock_acquire(&filesys_lock);
      	  file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
      	  lock_release(&filesys_lock);
      	}
      	free_page (pagedir_get_page(cur->pagedir, vme->vaddr));
      	pagedir_clear_page(cur->pagedir, vme->vaddr);
      	//list_remove(&vme->elem);
        hash_delete(&cur->vm, &vme->elem);
      	free(vme);
      }

      struct list_elem *next = list_next(it);

      file_close(mmap_f->file);
      list_remove(&mmap_f->elem);
      free(mmap_f);

      it = next;
	}
	else
	  it = list_next(it);
  }
}
