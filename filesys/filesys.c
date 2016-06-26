#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/buffer_cache.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  // bc_init ();

  if (format)
    do_format ();

  free_map_open ();

  // Subdirectory
  thread_current ()->cur_dir = dir_open_root ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  // bc_flush_all_entries ();
  bc_term ();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  block_sector_t inode_sector = 0;
  // Subdirectory TODO: malloc-free 대신 다른 방법 없을지 고민
  char *cp_name = malloc (sizeof(char) * (strlen (name) + 1));
  char *file_name = malloc (sizeof(char) * (NAME_MAX + 1));
  strlcpy (cp_name, name, strlen (name) + 1);
  struct dir *dir = parse_path (cp_name, file_name); //dir_open_root ();

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, 0)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  free (cp_name);
  free (file_name);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  // Subdirectory TODO: malloc-free 대신 다른 방법 없을지 고민
  char *cp_name = malloc (sizeof(char) * (strlen (name) + 1));
  char *file_name = malloc (sizeof(char) * (NAME_MAX + 1));
  strlcpy (cp_name, name, strlen (name) + 1);
  struct dir *dir = parse_path (cp_name, file_name); //dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);

  free (cp_name);
  free (file_name);

  if (inode_is_dir (inode)) {
    dir_close (thread_current ()->cur_dir);
    thread_current ()->cur = dir_open (inode);
  }

  struct file *file = file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  bool success = false;
  // Subdirectory TODO: malloc-free 대신 다른 방법 없을지 고민
  char *cp_name = malloc (sizeof(char) * (strlen (name) + 1));
  char *file_name = malloc (sizeof(char) * (NAME_MAX + 1));
  strlcpy (cp_name, name, strlen (name) + 1);
  struct dir *dir = parse_path (cp_name, file_name); //dir_open_root ();
  struct inode *inode;

  if (dir == NULL || !dir_lookup (dir, file_name, &inode))
    goto done;

  if (inode_is_dir (inode)) {
    struct dir *f_dir = dir_open (inode);
    if (dir_readdir (f_dir, cp_name)) { // cp_name instead of temp_str
      inode_close (inode);
      dir_close (f_dir);
      goto done;
    }
  }

  //bool success = dir != NULL && dir_remove (dir, file_name);
  success = dir_remove (dir, file_name);

 done:
  dir_close (dir);

  free (cp_name);
  free (file_name);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  // Subdirectory
  dir_add (dir_open_root (), ".", ROOT_DIR_SECTOR);
  dir_add (dir_open_root (), "..", ROOT_DIR_SECTOR);
  free_map_close ();
  printf ("done.\n");
}

// Subdirectory
struct dir*
parse_path (char *path_name, char *file_name)
{
  struct dir *dir;

  if (path_name == NULL || file_name == NULL)
    PANIC ("parse_path failed\n");

  if (strlen (path_name) == 0)
    return NULL;

  /* PATH_NAME의 절대/상대경로에 따른 디렉터리 정보 저장 (구현)*/
  if (path_name[0] == '/')
    dir = dir_open_root ();
  else
    dir = dir_reopen (thread_current ()->cur_dir);

  char *token, *nextToken, *savePtr;
  token = strtok_r (path_name, "/", &savePtr);
  nextToken = strtok_r (NULL, "/", &savePtr);
  while (token != NULL && nextToken != NULL) {
    struct inode *inode;
    /* dir에서 token이름의 파일을 검색하여 inode의 정보를 저장 */
    if (!dir_lookup (dir, token, &inode))
      return NULL;

    /* inode가 파일일 경우 NULL 반환 */
    if (!inode_is_dir (inode))
      return NULL;

    /* dir의 디렉터리 정보를 메모리에서 해지 */
    dir_close (dir);

    /* inode의 디렉터리 정보를 dir에 저장 */
    dir = dir_open (inode);

    /* token에 검색할 경로 이름 저장 */
    token = nextToken;
    nextToken = strtok_r (NULL, "/", &savePtr);

    inode_close (inode);
  }
  /* token의 파일 이름을 file_name에 저장 */
  strlcpy (file_name, token, strlen (token) + 1);

  /* dir 정보 반환 */
  return dir;
}

bool
filesys_create_dir (const char *name)
{
  block_sector_t sector_idx = 0, f_sector_idx;
  /* name 경로 분석 */
  // Subdirectory TODO: malloc-free 대신 다른 방법 없을지 고민
  char *cp_name = malloc (sizeof(char) * (strlen (name) + 1));
  char *file_name = malloc (sizeof(char) * (NAME_MAX + 1));
  strlcpy (cp_name, name, strlen (name) + 1);
  struct dir *dir = parse_path (cp_name, file_name);

  bool success = (dir != NULL
                  /* bitmap에서 inode sector 번호 할당 */
                  && free_map_allocate (1, &sector_idx)

                  /* 할당받은 sector에 file_name의 디렉터리 생성 */
                  // TODO: 두 번째 인자(length) 충분한 지 확인
                  && dir_create (sector_idx, 25)

                  /* 디렉터리 엔트리에 file_name의 엔트리 추가 */
                  && dir_add (dir, file_name, sector_idx));

  if (success) {
    f_sector_idx = inode_get_inumber (dir_get_inode (dir));
    struct inode *inode = inode_open (sector_idx);
    dir_close (dir);
    dir = dir_open (inode);

    success    = (success
                  /* 디렉터리 엔트리에 ‘.’, ‘..’ 파일의 엔트리 추가 */
                  && dir_add (dir, ".", sector_idx)
                  && dir_add (dir, "..", f_sector_idx));

    inode_close (inode);
  }
  if (!success && sector_idx != 0)
    free_map_release (sector_idx, 1);

  dir_close (dir);

  free (cp_name);
  free (file_name);

  return success;
}
