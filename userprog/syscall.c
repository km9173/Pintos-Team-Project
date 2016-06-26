#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
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
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)(arg[0]));
      status = *(int *)arg[0];
      exit(status);
      break;

    case SYS_EXEC:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)(arg[0]));
      cmd_line = *(char **)arg[0];
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
      f->eax = create(file, size);
      break;

    case SYS_REMOVE:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      file = *(char **)arg[0];
      f->eax = remove (file);
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
      get_argument (f->esp, (int *)arg, 3);
      chec_address((void *)arg[0]);
      chec_address((void *)arg[1]);
      chec_address((void *)arg[2]);
      fd = *(int *)arg[0];
      buffer = *(void **)arg[1];
      size = *(int *)arg[2];
      chec_address (buffer + size);
      f->eax = read (fd, buffer, size);
      break;

    case SYS_WRITE:
      get_argument (f->esp, (int *)arg, 3);
      chec_address((void *)arg[0]);
      chec_address((void *)arg[1]);
      chec_address((void *)arg[2]);
      fd = *(int *)arg[0];
      buffer = *(void **)arg[1];
      size = *(int *)arg[2];
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

    // Subdirectory
    case SYS_ISDIR:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      fd = *(int *)arg[0];
      f->eax = isdir (fd);
      break;

    case SYS_CHDIR:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      file = *(char **)arg[0];
      f->eax = chdir (file);
      break;

    case SYS_MKDIR:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      file = *(char **)arg[0];
      f->eax = mkdir (file);
      break;

    case SYS_READDIR:
      get_argument (f->esp, (int *)arg, 2);
      chec_address((void *)arg[0]);
      chec_address((void *)arg[1]);
      fd = *(int *)arg[0];
      file = *(char **)arg[1];
      f->eax = readdir (fd, file);
      break;

    case SYS_INUMBER:
      get_argument (f->esp, (int *)arg, 1);
      chec_address((void *)arg[0]);
      fd = *(int *)arg[0];
      f->eax = inumber (fd);
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
    esp = esp + 4;
    chec_address(esp);  // Check if *esp address in user memory area
    arg[i] = esp;
  }
}

void
chec_address (void *addr)
{
  // Check addr is user memory area, If invalid access then exit process
  if (addr < (void*) 0x8048000 || addr >= (void*) 0xc0000000)
    exit(-1);
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
    else if (inode_is_dir (file_get_inode(f)))
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
}

bool
isdir (int fd)
{
  return inode_is_dir (file_get_inode (process_get_file (fd)));
}

bool
chdir (const char *dir)
{
  // TODO: 이 코드가 맞는지, 잘 동작하는지 확인
  // TODO: malloc-free 대신 다른 방법 없을지 고민
  char *cp_name = malloc (sizeof(char) * (strlen (dir) + 1));
  char *file_name = malloc (sizeof(char) * (NAME_MAX + 1));
  strlcpy (cp_name, dir, strlen (dir) + 1);

  /* dir 경로를 분석하여 디렉터리를 반환 */
  struct dir *f_dir = parse_path (cp_name, file_name);
  struct inode *inode;

  // root 예외처리
  if (!dir_lookup (f_dir, file_name, &inode)) {
    if (inode_get_inumber (dir_get_inode (f_dir)) == ROOT_DIR_SECTOR && inode == NULL) {
      free (cp_name);
      free (file_name);
      dir_close (thread_current ()->cur_dir);
      thread_current ()->cur_dir = f_dir;
      return true;
    }
   else
    return false;
  }

  /* inode가 파일일 경우 NULL 반환 */
  if (!inode_is_dir (inode))
    return false;

  /* dir의 디렉터리 정보를 메모리에서 해지 */
  dir_close (f_dir);

  /* inode의 디렉터리 정보를 dir에 저장 */
  f_dir = dir_open (inode);

  free (cp_name);
  free (file_name);

  if (f_dir == NULL)
    return false;
  else {
    /* 스레드의 현재 작업 디렉터리를 변경 */
    dir_close (thread_current ()->cur_dir);
    thread_current ()->cur_dir = f_dir;
    return true;
  }
}

bool
mkdir (const char *dir)
{
  return filesys_create_dir (dir);
}

bool
readdir (int fd, char *name)
{
  char file_name[NAME_MAX + 1];
  int pos = 0;
  /* fd 리스트에서 fd에 대한 file 정보를 얻어옴 */
  struct file *p_file = process_get_file (fd);

  /* fd의 file의 inode가 디렉터리인지 검사 */
  if (!inode_is_dir (file_get_inode (p_file)))
    return false;

  /* p_file을 dir자료구조로 포인팅 */
  /* 디렉터리의 엔트에서 “.”,”..” 이름을 제외한 파일이름을 name에 저장 */
  // TODO: '\n'을 끝에 넣어주는 게 맞는 건지 모르겠음. 그렇다고 안 넣으면 \0인데?
  while (dir_readdir ((struct dir *)p_file, file_name)) {
    if (*file_name != '.')
      strlcpy (name + pos, file_name, strlen (file_name) + 1);
    pos += strlen (file_name) + 1;
    name[pos] = '\n';
    pos++;
  }
}

block_sector_t
inumber (int fd)
{
  return inode_get_inumber (file_get_inode (process_get_file (fd)));
}
