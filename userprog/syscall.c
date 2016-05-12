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
      exit(arg[0]);
      break;

    case SYS_EXEC:
      get_argument (f->esp, (int *)arg, 1);
      // check_address((void *)(arg[0]));
      check_valid_string ((const void *)arg[0], f->esp);
      // cmd_line = *(char **)arg[0];
      // 0: cmd_line
      f->eax = exec ((const char *)arg[0]);
      break;

    case SYS_WAIT:
      get_argument (f->esp, (int *)arg, 1);
      check_address((void *)arg[0], f->esp);
      // pid = *(int *)arg[0];
      // 0: pid
      f->eax = wait (arg[0]);
      break;

    case SYS_CREATE:
      get_argument (f->esp, (int *)arg, 2);
      // check_address((void *)arg[0]);
      check_valid_string ((const void *)arg[0], f->esp);
      check_address((void *)arg[1], f->esp);
      // file = *(char **)arg[0];
      // size = *(int *)arg[1];
      // 0: file, 1: size
      f->eax = create((const char *)arg[0], arg[1]);
      break;

    case SYS_REMOVE:
      get_argument (f->esp, (int *)arg, 1);
      // check_address((void *)arg[0]);
      check_valid_string ((const void *)arg[0], f->esp);
      // file = *(char **)arg[0];
      // 0: file
      f->eax = remove ((const char *)arg[0]);
      break;

    case SYS_OPEN:
      get_argument (f->esp, (int *)arg, 1);
      // check_address((void *)arg[0]);
      check_valid_string ((const void *)arg[0], f->esp);
      // file = *(char **)arg[0];
      // 0: file
      f->eax = open ((const char *)arg[0]);
      break;

    case SYS_FILESIZE:
      get_argument (f->esp, (int *)arg, 1);
      check_address((void *)arg[0], f->esp);
      // fd = *(int *)arg[0];
      // 0: fd
      f->eax = filesize (arg[0]);
      break;

    case SYS_READ:
      get_argument (f->esp, (int *)arg, 3);
      check_address((void *)arg[0], f->esp);
      // check_address((void *)arg[1]);
      check_address((void *)arg[2], f->esp);
      check_valid_buffer ((void *)arg[1], *(int *)arg[2], f->esp, true);
      // fd = *(int *)arg[0];
      // buffer = *(void **)arg[1];
      // size = *(int *)arg[2];
      // check_address (buffer + size);
      // 0: fd, 1: buffer, 2: size
      check_address ((void *)(arg[1] + arg[2]), f->esp);
      f->eax = read (arg[0], (void *)arg[1], (unsigned)arg[2]);
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
      check_address ((void *)(arg[1] + arg[2]), f->esp);
      f->eax = write (arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;

    case SYS_SEEK:
      get_argument (f->esp, (int *)arg, 2);
      check_address((void *)arg[0], f->esp);
      check_address((void *)arg[1], f->esp);
      // fd = *(int *)arg[0];
      // position = *(unsigned *)arg[1];
      // 0: fd, 1: position
      seek (arg[0], (unsigned)arg[1]);
      break;

    case SYS_TELL:
      get_argument (f->esp, (int *)arg, 1);
      check_address((void *)arg[0], f->esp);
      // fd = *(int *)arg[0];
      // 0: fd
      f->eax = tell (arg[0]);
      break;

    case SYS_CLOSE:
      get_argument (f->esp, (int *)arg, 1);
      check_address((void *)arg[0], f->esp);
      // fd = *(int *)arg[0];
      // 0: fd
      close (arg[0]);
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
    arg[i] = esp;
  }
}

struct vm_entry *
check_address (void *addr, void *esp UNUSED)
{
  struct vm_entry *vme = NULL;
  // Check addr is user memory area, If invalid access then exit process
  if (addr < (void *) 0x8048000 || addr >= (void *) 0xc0000000)
    exit(-1);

  vme = find_vme (addr);
  // TODO : refactoring 필요.
  // return vme; 로 줄여도 되지만, 일단은 가독성을 위해 아래와 같이 작성..
  if (vme != NULL)
    return vme;
  return NULL;
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
    exit(-1);

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

  f = filesys_open(file);

  if (f == NULL)
    return -1;

  while(t->fd_table[fd])
    fd++;

  t->fd_table[fd] = f;

  if (fd == t->fd_size)
    t->fd_size++;

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

  lock_acquire(&filesys_lock);

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
    f = process_get_file(fd);

    if (f == NULL)
      read_size = -1;
    else
      read_size = file_read(f, buffer, size);
  }

  lock_release(&filesys_lock);

  return read_size;
}

int
write (int fd, void *buffer, unsigned size)
{
  struct file *f = NULL;
  int read_size = 0;

  lock_acquire (&filesys_lock);

  if (fd == STDOUT_FILENO)
  {
    putbuf ((char *)buffer, size);
    read_size = size;
  }

  else
  {
    f = process_get_file (fd);
    if (f == NULL)
      read_size = -1;
    else
      read_size = file_write (f, buffer, size);
  }

  lock_release (&filesys_lock);

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
  struct thread *t = thread_current();

  process_close_file(fd);
  t->fd_size--;

// vm_entry
}
void
check_valid_buffer (void *buffer, unsigned size, void *esp, bool to_write)
{
  struct vm_entry *vme = check_address (buffer, esp);
  int count = 1;

  if (vme == NULL || to_write != vm->writable)
    exit (-1);

  while (size > count * 1024)
  {
    vme = check_address (buffer + count * 1024, esp);
    if (vme == NULL)
      exit (-1);
    count++;
  }
}

void
check_valid_string (const void *str, void *esp)
{
  if (check_address (str, esp) == NULL)
    exit (-1);
}
