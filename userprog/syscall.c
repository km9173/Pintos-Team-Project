#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <filesys/filesys.h>
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
void get_argument(void *esp, int *arg, int count);
void chec_address (void *addr);
void halt (void);
void exit (int status);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int status, fd, pid, arg[4];
  char *file;
  void *buffer;
  unsigned size, position;

  // Check if stack pointer is in the user memory area
  chec_address (f->esp);

  // Save user stack arguments in kernel
  switch (*(int *)(f->esp))
  {
  //f->eax = return Value;
    case SYS_HALT:
      halt ();
      break;

    case SYS_EXIT:
      get_argument (f->esp, &status, 1);
      exit(status);  // void exit (int status)
      break;

    case SYS_EXEC:
      get_argument (f->esp, arg, 1);
      exec (arg[0]);
      break;

    case SYS_WAIT:
      get_argument (f->esp, &pid, 1);
      wait (pid);
      break;

    case SYS_CREATE:
      get_argument(f->esp, arg, 2);
      file = arg[0];
      size = arg[1];
      chec_address(file);
      f->eax = create(file, size);  // bool create (const char *file, unsigned initial_size)
      break;

    case SYS_REMOVE:
      get_argument(f->esp, (int*)file, 1);
      chec_address(file);
      f->eax = remove(file);  // bool remove (const char *file)
      break;

    case SYS_OPEN:
      // chec_address(file);
      // int open (const char *file)
      break;

    case SYS_FILESIZE:
      //  int filesize (int fd)
      break;

    case SYS_READ:
      // chec_address(buffer); //I'm not sure whether buffer needs checking

      // int read (int fd, void *buffer, unsigned size)
      // TODO: 파일에 대한 접근이 끝난뒤 lock 해제
      lock_release(&filesys_lock);
      break;

    case SYS_WRITE:
      // chec_address(buffer); //I'm not sure whether buffer needs checking
      // TODO: 파일에 접근하기 전에 lock 획득 기능 추가
      lock_acquire(&filesys_lock);
      // int write (int fd, const void *buffer, unsigned size)
      // TODO: 파일에 대한 접근이 끝난뒤 lock 해제
      lock_release(&filesys_lock);
      break;

    case SYS_SEEK:
      // void seek (int fd, unsigned position)
      break;

    case SYS_TELL:
      // unsigned tell (int fd)
      break;

    case SYS_CLOSE:
      // void closd (int fd)
      break;

    default:
      thread_exit ();
  }

  // delete when implementation finished
  thread_exit ();
}

void
get_argument (void *esp, int *arg, int count)
{
  int i = 0;
  for (i = 0; i < count; i++)
  {
    esp = esp + 4;
    chec_address(esp);  // Check if *esp address in user memory area
    arg[i] = *(int *)esp;
  }
}

void
chec_address (void *addr)
{
  // Check addr is user memory area, If invalid access then exit process
  if (addr < (void*) 0x8048000 || addr > (void*) 0xc0000000)
  {
    printf("TODO: exit(-1) call\n");
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

tid_t
exec (const *cmd_line)
{
  tid_t pid;
  struct thread *child_process;
  struct thread *t = thread_current();

  pid = process_execute(cmd_line);
  child_process = get_child_process(pid);

  sema_down(&(t->load));

  if (!t->memory_load_success)
    return -1;

  return pid;
}

int
wait (tid_t tid)
{
  return process_wait(tid);
}

bool
create (const char *file, unsigned initial_size)
{
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
  struct file *_file = filesys_open(file);
  int fd = t->fd_size;

  if (_file == NULL)
    return -1;

  while(t->fd_table[fd] != NULL)
    fd++;

  t->fd_table[fd] = _file;
  t->fd_size++;

  return fd;
}

int
filesize (int fd)
{

}

int
read (int fd, void *buffer, unsigned size)
{
  struct thread *t = thread_current();
  struct file *file = t->fd_table[fd];
  int i = 0;
  char key;

  // TODO: 파일에 접근하기 전에 lock 획득 기능 추가
  // 만약 lock을 획득할수 없다면 어떻게 되는거지 ..
  // 심지어 lock_acquire 반환값도 없음..
  lock_acquire(&filesys_lock);

  if (fd == 0)
  {
    while((key = input_getc()) != '\r')
    {
      ((char*)buffer)[i] = key;
      i++;
    }
    // TODO: 문자열 맨마지막에 '\0'를 넣어야하나?
    return i;
  }

  // TODO: 읽기 실패시 -1 반환 부분을 어디서 구현할지..
  else
    return file_read(file, buffer, size);
}

int
write (int fd, void *buffer, unsigned size)
{

}

void
seek (int fd, unsigned position)
{

}

unsigned
tell (int fd)
{
  struct thread *t = thread_current();
  struct file *_file = t->fd_table[fd];

  return _file->pos;
}

void
close (int fd)
{
  struct thread *t = thread_current();

  process_close_file(fd);
  t->fd_table[fd] = NULL;
  t->fd_size--;
}
