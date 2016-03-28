#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

struct lock filesys_lock;

void syscall_init (void);
// project_2
void halt (void);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
// project_3
void exit (int status);
tid_t exec (const *cmd_line);
int wait (tid_t);
// project_4
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

#endif /* userprog/syscall.h */
