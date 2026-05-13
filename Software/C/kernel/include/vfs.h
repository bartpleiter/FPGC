/*
 * vfs.h — Virtual File System with operations vtable.
 *
 * Each open file has a pointer to a file_ops struct that dispatches
 * read/write/close/etc. to the correct device driver. New devices
 * are added by implementing file_ops and registering an open handler.
 */
#ifndef KERNEL_VFS_H
#define KERNEL_VFS_H

/* Forward declaration — full definition in proc.h */
struct proc;

/* File open flags (matches userlib conventions) */
#define O_RDONLY    1
#define O_WRONLY    2
#define O_RDWR     3
#define O_APPEND    4
#define O_CREAT     8
#define O_TRUNC    16
#define O_RAW      32
#define O_NONBLOCK 64

/* Seek whence */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Maximum open files system-wide */
#define MAX_OPEN_FILES  64

struct open_file;

/*
 * Operations vtable — each device/file type implements these.
 * Return values: >=0 on success, -1 on error.
 * NULL function pointers mean "not supported" (returns -1).
 */
struct file_ops {
    int  (*read)(struct open_file *f, void *buf, int count);
    int  (*write)(struct open_file *f, const void *buf, int count);
    int  (*lseek)(struct open_file *f, int offset, int whence);
    int  (*close)(struct open_file *f);
    int  (*ioctl)(struct open_file *f, int cmd, int arg);
};

/*
 * Global open file table entry.
 * Per-process fd tables store indices into this table.
 */
struct open_file {
    int             refcount;   /* Number of fds pointing here */
    struct file_ops *ops;       /* Device-specific operations */
    void           *private;    /* Device-specific state */
    int             flags;      /* O_RDONLY, O_WRONLY, etc. */
    int             pos;        /* Current position (seekable files) */
};

/* ---- VFS API (called by syscall dispatcher) ---- */

void vfs_init(void);

/* Open a path, returning a global file table index, or -1 on error. */
int vfs_open(const char *path, int flags);

/* Read/write/close/seek/ioctl on a global file table index. */
int vfs_read(int gfd, void *buf, int count);
int vfs_write(int gfd, const void *buf, int count);
int vfs_lseek(int gfd, int offset, int whence);
int vfs_close(int gfd);
int vfs_ioctl(int gfd, int cmd, int arg);

/* Path operations (not fd-based) */
int vfs_unlink(const char *path);
int vfs_mkdir(const char *path);
int vfs_readdir(const char *path, void *buf, int max);
int vfs_stat(const char *path, void *buf);
int vfs_rename(const char *oldpath, const char *newpath);

/* ---- Per-process fd layer ---- */

/* Translate process-local fd to global file table index. */
int fd_to_gfd(int fd);

/* Allocate a process-local fd pointing to a global file. */
int fd_alloc(int gfd);

/* Close a process-local fd (decrements refcount). */
int fd_close(int fd);

/* Duplicate a fd: make newfd point to the same file as oldfd. */
int fd_dup2(int oldfd, int newfd);

/* Initialize fd table for a new process (opens stdin/stdout/stderr). */
void fd_init_stdio(void);

/* Inherit fd table from parent process. */
void fd_inherit(struct proc *child, struct proc *parent);

/* Close all fds for a process (on exit). */
void fd_close_all(void);

/* ---- Device registration ---- */

/*
 * Register a device open handler. When vfs_open() matches the
 * given path prefix, it calls the handler to create the open_file.
 * Returns 0 on success, -1 if table full.
 */
int vfs_register_device(const char *prefix, struct file_ops *ops,
                        int (*open_fn)(const char *path, int flags,
                                       struct open_file *f));

#endif /* KERNEL_VFS_H */
