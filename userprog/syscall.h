#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

#define CLOSE_ALL -1

typedef int pid_t;
typedef int mapid_t;

struct lock filesys_lock;

void syscall_init (void);
// project_2
void halt (void);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
// project_3
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t);
// project_4
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
mapid_t mmap (int fd, void *addr);
void munmap (int mapid);

#endif /* userprog/syscall.h */
