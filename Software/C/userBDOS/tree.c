// tree.c — Display directory tree for FPGC BDOS
// Usage: tree [path]
// Defaults to current working directory if no path given.

#define USER_SYSCALL
#define COMMON_STRING
#define COMMON_STDLIB
#include "libs/user/user.h"
#include "libs/common/common.h"

/*===========================================================================*/
/*  Constants                                                                */
/*===========================================================================*/

#define MAX_ENTRIES     64   // Max entries per directory listing
#define DIR_ENTRY_WORDS 8    // sizeof(brfs_dir_entry) in words
#define FILENAME_WORDS  4    // Compressed filename: 4 words
#define MAX_PATH_LEN    256
#define MAX_DEPTH       16   // Maximum recursion depth
#define MAX_NAME_LEN    17   // 16 chars + null terminator

/* Offsets within a dir_entry (in words) */
#define ENTRY_FILENAME  0    // words 0-3: compressed filename
#define ENTRY_FLAGS     5    // word 5: flags
#define ENTRY_FILESIZE  7    // word 7: filesize

#define FLAG_DIRECTORY  0x01

/* Box-drawing characters (CP437) */
#define CH_VERT    179  /* │ */
#define CH_CORNER  192  /* └ */
#define CH_TEE     195  /* ├ */
#define CH_HORIZ   196  /* ─ */

/*===========================================================================*/
/*  Globals                                                                  */
/*===========================================================================*/

unsigned int *entry_buf;   // Heap buffer for directory entries
int dir_count;             // Total directories found
int file_count;            // Total files found

/*===========================================================================*/
/*  Helpers                                                                  */
/*===========================================================================*/

void print_char(int ch)
{
  sys_print_char(ch);
}

void print_str(char *s)
{
  sys_print_str(s);
}

/* Decompress BRFS filename from 4 packed words into a C string */
void decompress_name(char *dest, unsigned int *src)
{
  int wi;
  int ci;
  unsigned int word;
  unsigned int c;

  ci = 0;
  for (wi = 0; wi < FILENAME_WORDS; wi++)
  {
    word = src[wi];

    c = (word >> 24) & 0xFF;
    dest[ci] = c;
    ci++;
    if (c == 0) return;

    c = (word >> 16) & 0xFF;
    dest[ci] = c;
    ci++;
    if (c == 0) return;

    c = (word >> 8) & 0xFF;
    dest[ci] = c;
    ci++;
    if (c == 0) return;

    c = word & 0xFF;
    dest[ci] = c;
    ci++;
    if (c == 0) return;
  }
  dest[ci] = 0;
}

/* Get the base pointer for entry i in entry_buf */
unsigned int *entry_at(int i)
{
  return entry_buf + (i * DIR_ENTRY_WORDS);
}

/* Check if entry i is a directory */
int entry_is_dir(int i)
{
  return (entry_at(i)[ENTRY_FLAGS] & FLAG_DIRECTORY) ? 1 : 0;
}

/* Get name of entry i into dest */
void entry_name(int i, char *dest)
{
  decompress_name(dest, entry_at(i) + ENTRY_FILENAME);
}

/* Simple bubble sort for names array with parallel types array */
void sort_entries(char names[][MAX_NAME_LEN], int types[], int count)
{
  int i;
  int j;
  char tmp_name[MAX_NAME_LEN];
  int tmp_type;

  for (i = 0; i < count - 1; i++)
  {
    for (j = 0; j < count - 1 - i; j++)
    {
      /* Directories first, then alphabetical */
      if (types[j] == 0 && types[j + 1] == 1)
      {
        /* file before dir: swap */
        strcpy(tmp_name, names[j]);
        strcpy(names[j], names[j + 1]);
        strcpy(names[j + 1], tmp_name);
        tmp_type = types[j];
        types[j] = types[j + 1];
        types[j + 1] = tmp_type;
      }
      else if (types[j] == types[j + 1] && strcmp(names[j], names[j + 1]) > 0)
      {
        /* same type, alphabetical */
        strcpy(tmp_name, names[j]);
        strcpy(names[j], names[j + 1]);
        strcpy(names[j + 1], tmp_name);
        tmp_type = types[j];
        types[j] = types[j + 1];
        types[j + 1] = tmp_type;
      }
    }
  }
}

/*===========================================================================*/
/*  Tree printing                                                            */
/*===========================================================================*/

/* Print the prefix for the current depth level.
 * continues[i] == 1 means level i has more siblings below, so print "│   ".
 * continues[i] == 0 means level i was the last entry, so print "    ".
 */
void print_prefix(int *continues, int depth)
{
  int i;
  for (i = 0; i < depth; i++)
  {
    if (continues[i])
    {
      print_char(CH_VERT);
      print_char(' ');
    }
    else
    {
      print_str("  ");
    }
  }
}

/* Print tree connector for an entry */
void print_connector(int is_last)
{
  if (is_last)
  {
    print_char(CH_CORNER);
    print_char(CH_HORIZ);
  }
  else
  {
    print_char(CH_TEE);
    print_char(CH_HORIZ);
  }
}

/* Recursively list directory contents as a tree */
void tree_dir(char *path, int *continues, int depth)
{
  /* Use heap-allocated arrays to avoid stack overflow.
   * We allocate per-call so recursive calls each get their own buffers.
   */
  char (*names)[MAX_NAME_LEN];
  int *types; /* 1 = directory, 0 = file */
  int count;
  int valid;
  int i;
  int is_last;
  char name[MAX_NAME_LEN];
  char child_path[MAX_PATH_LEN];

  if (depth >= MAX_DEPTH)
  {
    return;
  }

  /* Read directory entries using the syscall */
  count = sys_fs_readdir(path, entry_buf, MAX_ENTRIES);
  if (count <= 0)
  {
    return;
  }

  /* Allocate arrays for this directory's contents */
  names = (char (*)[MAX_NAME_LEN])sys_heap_alloc(MAX_ENTRIES * MAX_NAME_LEN);
  types = (int *)sys_heap_alloc(MAX_ENTRIES);
  if (names == 0 || types == 0)
  {
    print_str("[heap exhausted]\n");
    return;
  }

  /* Collect valid entries (skip . and ..) */
  valid = 0;
  for (i = 0; i < count; i++)
  {
    entry_name(i, name);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
      continue;
    }

    strcpy(names[valid], name);
    types[valid] = entry_is_dir(i);
    valid++;
  }

  /* Sort: directories first, then alphabetical */
  sort_entries(names, types, valid);

  /* Print each entry */
  for (i = 0; i < valid; i++)
  {
    is_last = (i == valid - 1) ? 1 : 0;

    print_prefix(continues, depth);
    print_connector(is_last);
    print_str(names[i]);
    print_char('\n');

    if (types[i])
    {
      /* It's a directory — recurse */
      dir_count++;

      /* Build child path */
      strcpy(child_path, path);
      if (strlen(path) > 1)
      {
        strcat(child_path, "/");
      }
      strcat(child_path, names[i]);

      continues[depth] = is_last ? 0 : 1;
      tree_dir(child_path, continues, depth + 1);
    }
    else
    {
      file_count++;
    }
  }
}

/*===========================================================================*/
/*  Main                                                                     */
/*===========================================================================*/

int main()
{
  int argc;
  char **argv;
  char *cwd;
  char root_path[MAX_PATH_LEN];
  int continues[MAX_DEPTH];
  int i;

  argc = sys_shell_argc();
  argv = sys_shell_argv();
  cwd = sys_shell_getcwd();

  if (argc > 2)
  {
    print_str("usage: tree [path]\n");
    return 1;
  }

  /* Determine root path */
  if (argc == 2)
  {
    if (argv[1][0] == '/')
    {
      strcpy(root_path, argv[1]);
    }
    else
    {
      strcpy(root_path, cwd);
      if (strlen(cwd) > 1)
      {
        strcat(root_path, "/");
      }
      strcat(root_path, argv[1]);
    }
  }
  else
  {
    strcpy(root_path, cwd);
  }

  /* Allocate entry buffer on heap (shared across all recursion levels —
   * each level reads into it and extracts data before recursing) */
  entry_buf = sys_heap_alloc(MAX_ENTRIES * DIR_ENTRY_WORDS);
  if (entry_buf == 0)
  {
    print_str("error: heap alloc failed\n");
    return 1;
  }

  /* Initialize */
  dir_count = 0;
  file_count = 0;
  for (i = 0; i < MAX_DEPTH; i++)
  {
    continues[i] = 0;
  }

  /* Print root */
  print_str(root_path);
  print_char('\n');

  /* Walk the tree */
  tree_dir(root_path, continues, 0);

  /* Print summary */
  print_char('\n');

  /* Print directory count */
  {
    char buf[16];
    itoa(dir_count, buf, 10);
    print_str(buf);
  }
  if (dir_count == 1)
  {
    print_str(" directory, ");
  }
  else
  {
    print_str(" directories, ");
  }

  /* Print file count */
  {
    char buf[16];
    itoa(file_count, buf, 10);
    print_str(buf);
  }
  if (file_count == 1)
  {
    print_str(" file\n");
  }
  else
  {
    print_str(" files\n");
  }

  return 0;
}

void interrupt()
{
  // Required empty interrupt handler
}
