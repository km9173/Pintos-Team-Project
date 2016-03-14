#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
void chec_address (void *addr);
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
    chec_address(esp);
    arg[i] = *(int *)esp;
    esp = esp + 4;
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
