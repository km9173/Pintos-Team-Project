#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>

static void syscall_handler (struct intr_frame *);
void get_argument(void *esp, int *arg, int count);
void chec_address (void *addr);
void halt (void);
void exit (int status);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);




void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
  //for test
  get_argument (f->esp, &status, 1);

  // Save user stack arguments in kernel
  switch (*(int *)(f->esp))
  {
  //f->eax = return Value;
    case SYS_HALT:
      halt ();
      break;
    case SYS_EXIT:
    //  void exit (int status)
      get_argument (f->esp, &status, 1);
      printf ("syscall_exit!\n");
      break;
    /*
    case SYS_EXEC:
    //  pid_t exec (const char *cmd_line)
    case SYS_WAIT:
    //  int wait (pid_t pid)
    */
    case SYS_CREATE:
    //  bool create (const char *file, unsigned initial_size)
      get_argument(f->esp, arg, 2);
      file = arg[0];
      size = arg[1];
      chec_address(file);
      f->eax = create(file, size);
      break;
    case SYS_REMOVE:
    //  bool remove (const char *file)
      chec_address(file);
      printf ("syscall_remove!\n");
      break;
    /*
    case SYS_OPEN:
      chec_address(file);
    //  int open (const char *file)
      break;
    case SYS_FILESIZE:
    //  int filesize (int fd)
      break;
    case SYS_READ:
      chec_address(buffer); //I'm not sure whether buffer needs checking
    //  int read (int fd, void *buffer, unsigned size)
      break;
    case SYS_WRITE:
      chec_address(buffer); //I'm not sure whether buffer needs checking
    //  int write (int fd, const void *buffer, unsigned size)
      break;
    case SYS_SEEK:
    //  void seek (int fd, unsigned position)
      break;
    case SYS_TELL:
    //  unsigned tell (int fd)
      break;
    case SYS_CLOSE:
    //  void closd (int fd)
      break;
    */
    default:
      printf ("system call!\n");
      thread_exit ();
  }

  // delete when implementation finished
  thread_exit ();
}

void
get_argument (void *esp, int *arg, int count)
{
  int i = 0;
  printf("get argument func call!\n");
  for (i = 0; i < count; i++)
  {
    esp = esp + 4;
    // TODO: check if *esp address in user memory area
    chec_address(esp);
    arg[i] = *(int *)esp;
  }
}

void
chec_address (void *addr)
{
  // TODO: check addr is user memory area
  // TODO: if invalid access then exit process
  if (addr < (void*) 0x8048000 || addr > (void*) 0xc0000000)
    printf("TODO: exit(-1) call\n"); //exit(-1);
}

void
halt (void)
{
  shutdown_power_off();
}

void
exit(int status)
{

}

bool
create (const char *file, unsigned initial_size)
{
  return filesys_create(file, initial_size);
}

bool
remove (const char *file)
{

}
