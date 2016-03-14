#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
void get_argument(void *esp, int *arg, int count);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  printf ("system call!\n");
  thread_exit ();
}

void
get_argument (void *esp, int *arg, int count)
{
  int i = 0;
  printf("get argument func call!\n");
  for(i = 0; i < count; i++)
  {
    // TODO: check if *esp address in user memory area
    // chec_address(esp);
    arg[i] = *esp;
    esp = esp + 4;
  }
}
