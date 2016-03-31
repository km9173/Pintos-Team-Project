#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include "devices/input.h"
#include "filesys/filesys.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
void get_argument(void *esp, int *arg, int count);
void chec_address (void *addr);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int status, fd, pid;
  int *arg[4] = {0, };
  char *file = NULL, *cmd_line = NULL;
  void *buffer;
  unsigned size, position;

  //printf("1\n");
  // Check if stack pointer is in the user memory area
  chec_address (f->esp);
  //printf("2\n");
  //printf("call num : %d\n", *(int *)(f->esp));
  //printf("SYS_WRITE : %d\n", SYS_WRITE);

  // Save user stack arguments in kernel
  switch (*(int *)(f->esp))
  {
  //f->eax = return Value;
    case SYS_HALT:
      halt ();
      break;

    case SYS_EXIT:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)(arg[0]));
      status = *(int *)arg[0];
      //printf("[SYS_EXIT] status code : %d\n", *status);
      exit(status);
      break;

    case SYS_EXEC:
      // printf("hello SYS_EXEC!\n");
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)(arg[0]));
      cmd_line = *(char **)arg[0];
      // printf("cmd_line : %s\n", cmd_line);
      f->eax = exec (cmd_line);
      break;

    case SYS_WAIT:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      pid = *(int *)arg[0];
      f->eax = wait (pid);
      break;

    case SYS_CREATE:
      get_argument (f->esp, (int *)arg, 2);
      chec_address((void *)arg[0]);
      chec_address((void *)arg[1]);
      file = *(char **)arg[0];
      size = *(int *)arg[1];
      f->eax = create(file, size);  // bool create (const char *file, unsigned initial_size)
      break;

    case SYS_REMOVE:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      file = *(char **)arg[0];
      f->eax = remove (file);  // bool remove (const char *file)
      break;

    case SYS_OPEN:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      file = *(char **)arg[0];
      f->eax = open (file);
      break;

    case SYS_FILESIZE:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      fd = *(int *)arg[0];
      f->eax = filesize (fd);
      break;

    case SYS_READ:
      // printf("[SYS_READ] hello :)\n");
      get_argument (f->esp, (int *)arg, 3);
      chec_address((void *)arg[0]);
      chec_address((void *)arg[1]);
      chec_address((void *)arg[2]);
      fd = *(int *)arg[0];
      buffer = *(void **)arg[1];
      size = *(int *)arg[2];
      // printf("fd address : %p\n fd value : %d\n", arg[0], fd);
      chec_address (buffer + size);
      f->eax = read (fd, buffer, size);
      break;

    case SYS_WRITE:
      // printf("[SYS_WRITE] hello :)\n");
      get_argument (f->esp, (int *)arg, 3);
      chec_address((void *)arg[0]);
      chec_address((void *)arg[1]);
      chec_address((void *)arg[2]);
      fd = *(int *)arg[0];
      buffer = *(void **)arg[1];
      size = *(int *)arg[2];
      // printf("fd address : %p\n fd value : %d\n", arg[0], fd);
      chec_address (buffer + size);
      f->eax = write (fd, buffer, size);
      break;

    case SYS_SEEK:
      get_argument (f->esp, (int *)arg, 2);
      chec_address((void *)arg[0]);
      chec_address((void *)arg[1]);
      fd = *(int *)arg[0];
      position = *(unsigned *)arg[1];
      seek (fd, position);
      break;

    case SYS_TELL:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      fd = *(int *)arg[0];
      f->eax = tell (fd);
      break;

    case SYS_CLOSE:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      fd = *(int *)arg[0];
      close (fd);
      break;

    default:
      thread_exit ();
  }

  // delete when implementation finished
  //thread_exit ();
}

void
get_argument (void *esp, int *arg, int count)
{
  int i = 0;
  for (i = 0; i < count; i++)
  {
    // printf("3\n");
    esp = esp + 4;
    // printf("4\n");
    chec_address(esp);  // Check if *esp address in user memory area
    // printf("5\n");
    arg[i] = esp;
    // printf("arg[%d] address : %p\n", i, &arg[i]);
    // printf("arg[%d] value : %x\n", i, arg[i]);
    //printf("arg[%d] pointing value : %d\n", i, *arg[i]);
  }
}

void
chec_address (void *addr)
{
  // printf("addr : %p\n", addr);
  // printf("addr value addr : %x\n", *(int *)addr);
  // Check addr is user memory area, If invalid access then exit process
  if (addr < (void*) 0x8048000 || addr >= (void*) 0xc0000000)
  {
    exit(-1);
  }
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
  struct thread *t = thread_current();

  //printf("[exec] cmd_line : %s\n", cmd_line);
  // printf("1\n");
  pid = process_execute(cmd_line);
  // printf("2\n");
  child_process = get_child_process(pid);
  // printf("3\n");

  sema_down(&child_process->load);
  // printf("4\n");

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
    {
      file_write (f, buffer, size);
      read_size = size;
    }
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
}
