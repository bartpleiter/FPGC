/*
 * vfs.c — Virtual File System core.
 *
 * Global open file table + device registration + path dispatch.
 * Per-process fd mapping is handled via proc.fds[].
 */
#include "kernel.h"

/* Global open file table */
static struct open_file file_table[MAX_OPEN_FILES];

/* Device registration table */
#define MAX_DEVICES 16

struct dev_entry {
    const char    *prefix;
    int            prefix_len;
    struct file_ops *ops;
    int          (*open_fn)(const char *path, int flags,
                            struct open_file *f);
};

static struct dev_entry devices[MAX_DEVICES];
static int device_count;

/* ---- Initialization ---- */

void vfs_init(void)
{
    int i;
    for (i = 0; i < MAX_OPEN_FILES; i++)
    {
        file_table[i].refcount = 0;
        file_table[i].ops = 0;
        file_table[i].private = 0;
    }
    device_count = 0;
}

/* ---- Device registration ---- */

int vfs_register_device(const char *prefix, struct file_ops *ops,
                        int (*open_fn)(const char *path, int flags,
                                       struct open_file *f))
{
    int len;
    const char *p;

    if (device_count >= MAX_DEVICES) return -1;

    len = 0;
    p = prefix;
    while (*p) { len++; p++; }

    devices[device_count].prefix = prefix;
    devices[device_count].prefix_len = len;
    devices[device_count].ops = ops;
    devices[device_count].open_fn = open_fn;
    device_count++;
    return 0;
}

/* ---- Global file table helpers ---- */

static int gfd_alloc(void)
{
    int i;
    for (i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (file_table[i].refcount == 0)
            return i;
    }
    return -1;
}

/* ---- VFS open ---- */

int vfs_open(const char *path, int flags)
{
    int i;
    int gfd;
    int result;

    if (!path) return -1;

    gfd = gfd_alloc();
    if (gfd < 0) return -1;

    /* Check registered devices first */
    for (i = 0; i < device_count; i++)
    {
        int match;
        int j;

        match = 1;
        for (j = 0; j < devices[i].prefix_len; j++)
        {
            if (path[j] != devices[i].prefix[j])
            {
                match = 0;
                break;
            }
        }

        if (match)
        {
            file_table[gfd].refcount = 1;
            file_table[gfd].flags = flags;
            file_table[gfd].pos = 0;
            file_table[gfd].ops = devices[i].ops;
            file_table[gfd].private = 0;

            result = devices[i].open_fn(path, flags, &file_table[gfd]);
            if (result < 0)
            {
                file_table[gfd].refcount = 0;
                return -1;
            }
            return gfd;
        }
    }

    /* No device matched — try filesystem */
    {
        const char *rel_path;
        struct brfs_state *fs;
        int brfs_fd;

        fs = fs_for_path(path, &rel_path);
        if (!fs) return -1;

        if (flags & O_CREAT)
        {
            /* Try to create if it doesn't exist */
            if (!brfs_exists(fs, rel_path))
            {
                int create_result;
                create_result = brfs_create_file(fs, rel_path);
                if (create_result < 0)
                {
                    return -1;
                }
            }
        }

        brfs_fd = brfs_open(fs, rel_path);
        if (brfs_fd < 0)
            return -1;

        if (flags & O_APPEND)
        {
            int fsize;
            fsize = brfs_file_size(fs, brfs_fd);
            if (fsize > 0)
                brfs_seek(fs, brfs_fd, (unsigned int)fsize);
        }

        /* Store fs pointer and brfs fd in the open_file */
        file_table[gfd].refcount = 1;
        file_table[gfd].flags = flags;
        file_table[gfd].pos = 0;
        file_table[gfd].private = (void *)((unsigned int)brfs_fd |
                                           ((unsigned int)(fs == &brfs_sd) << 16));

        /* Set up file ops for BRFS files */
        extern struct file_ops fs_file_ops;
        file_table[gfd].ops = &fs_file_ops;

        return gfd;
    }
}

/* ---- VFS read/write/close/seek/ioctl ---- */

int vfs_read(int gfd, void *buf, int count)
{
    struct open_file *f;
    if (gfd < 0 || gfd >= MAX_OPEN_FILES) return -1;
    f = &file_table[gfd];
    if (f->refcount <= 0 || !f->ops || !f->ops->read) return -1;
    return f->ops->read(f, buf, count);
}

int vfs_write(int gfd, const void *buf, int count)
{
    struct open_file *f;
    if (gfd < 0 || gfd >= MAX_OPEN_FILES) return -1;
    f = &file_table[gfd];
    if (f->refcount <= 0 || !f->ops || !f->ops->write) return -1;
    return f->ops->write(f, buf, count);
}

int vfs_lseek(int gfd, int offset, int whence)
{
    struct open_file *f;
    if (gfd < 0 || gfd >= MAX_OPEN_FILES) return -1;
    f = &file_table[gfd];
    if (f->refcount <= 0 || !f->ops || !f->ops->lseek) return -1;
    return f->ops->lseek(f, offset, whence);
}

int vfs_close(int gfd)
{
    struct open_file *f;
    int result;

    if (gfd < 0 || gfd >= MAX_OPEN_FILES) return -1;
    f = &file_table[gfd];
    if (f->refcount <= 0) return -1;

    f->refcount--;
    if (f->refcount == 0)
    {
        result = 0;
        if (f->ops && f->ops->close)
            result = f->ops->close(f);
        f->ops = 0;
        f->private = 0;
        return result;
    }
    return 0;
}

int vfs_ioctl(int gfd, int cmd, int arg)
{
    struct open_file *f;
    if (gfd < 0 || gfd >= MAX_OPEN_FILES) return -1;
    f = &file_table[gfd];
    if (f->refcount <= 0 || !f->ops || !f->ops->ioctl) return -1;
    return f->ops->ioctl(f, cmd, arg);
}

void vfs_addref(int gfd)
{
    if (gfd >= 0 && gfd < MAX_OPEN_FILES)
        file_table[gfd].refcount++;
}

/* ---- Path operations ---- */

int vfs_unlink(const char *path)
{
    const char *rel;
    struct brfs_state *fs;
    fs = fs_for_path(path, &rel);
    if (!fs) return -1;
    return brfs_delete(fs, rel);
}

int vfs_mkdir(const char *path)
{
    const char *rel;
    struct brfs_state *fs;
    fs = fs_for_path(path, &rel);
    if (!fs) return -1;
    return brfs_create_dir(fs, rel);
}

/* Helper: create a synthetic directory entry */
static void vfs_synth_dir(struct brfs_dir_entry *e, const char *name)
{
    brfs_compress_string(e->filename, name);
    e->modify_date = 0;
    e->flags = BRFS_FLAG_DIRECTORY;
    e->fat_idx = 0;
    e->filesize = 0;
}

/* Helper: create a synthetic file entry */
static void vfs_synth_file(struct brfs_dir_entry *e, const char *name)
{
    brfs_compress_string(e->filename, name);
    e->modify_date = 0;
    e->flags = 0;
    e->fat_idx = 0;
    e->filesize = 0;
}

/* Check if two strings are equal */
static int vfs_streq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

int vfs_readdir(const char *path, void *buf, int max)
{
    struct brfs_dir_entry *entries;
    int count;

    entries = (struct brfs_dir_entry *)buf;
    count = 0;

    /* Root directory: BRFS entries + synthetic mount points */
    if (vfs_streq(path, "/"))
    {
        const char *rel;
        struct brfs_state *fs;
        int brfs_count;

        fs = fs_for_path(path, &rel);
        if (fs)
        {
            brfs_count = brfs_read_dir(fs, rel, entries, (unsigned int)max);
            if (brfs_count > 0)
                count = brfs_count;
        }

        /* Append synthetic mount points */
        if (count < max)
            vfs_synth_dir(&entries[count++], "dev");
        if (count < max)
            vfs_synth_dir(&entries[count++], "proc");
        if (count < max && fs_sd_ready)
            vfs_synth_dir(&entries[count++], "sdcard");
        return count;
    }

    /* /dev or /dev/ — list registered devices */
    if (vfs_streq(path, "/dev") || vfs_streq(path, "/dev/"))
    {
        int i;
        for (i = 0; i < device_count && count < max; i++)
        {
            /* Extract basename: skip "/dev/" prefix */
            const char *name;
            name = devices[i].prefix;
            if (name[0] == '/' && name[1] == 'd' && name[2] == 'e'
                && name[3] == 'v' && name[4] == '/')
                name = name + 5;
            /* Skip /proc/ entries (they have their own listing) */
            if (name[0] == '/' && name[1] == 'p')
                continue;
            vfs_synth_file(&entries[count++], name);
        }
        return count;
    }

    /* /proc or /proc/ — list proc virtual files */
    if (vfs_streq(path, "/proc") || vfs_streq(path, "/proc/"))
    {
        if (count < max) vfs_synth_file(&entries[count++], "uptime");
        if (count < max) vfs_synth_file(&entries[count++], "meminfo");
        if (count < max) vfs_synth_file(&entries[count++], "ps");
        if (count < max) vfs_synth_file(&entries[count++], "df");
        return count;
    }

    /* /sdcard or /sdcard/ — list SD card root directory */
    if (vfs_streq(path, "/sdcard") || vfs_streq(path, "/sdcard/"))
    {
        if (!fs_sd_ready) return -1;
        return brfs_read_dir(&brfs_sd, "", entries, (unsigned int)max);
    }

    /* Default: delegate to BRFS */
    {
        const char *rel;
        struct brfs_state *fs;
        fs = fs_for_path(path, &rel);
        if (!fs) return -1;
        return brfs_read_dir(fs, rel, entries, (unsigned int)max);
    }
}

int vfs_stat(const char *path, void *buf)
{
    /* TODO: implement stat */
    return -1;
}

int vfs_rename(const char *oldpath, const char *newpath)
{
    /* TODO: implement rename */
    return -1;
}

/* ---- Per-process fd layer ---- */

int fd_to_gfd(int fd)
{
    struct proc *p;
    if (fd < 0 || fd >= MAX_FDS) return -1;
    p = proc_current();
    if (!p) return -1;
    return p->fds[fd];
}

int fd_alloc(int gfd)
{
    struct proc *p;
    int i;
    p = proc_current();
    if (!p) return -1;
    for (i = 0; i < MAX_FDS; i++)
    {
        if (p->fds[i] < 0)
        {
            p->fds[i] = gfd;
            return i;
        }
    }
    return -1;
}

int fd_close(int fd)
{
    struct proc *p;
    int gfd;
    if (fd < 0 || fd >= MAX_FDS) return -1;
    p = proc_current();
    if (!p) return -1;
    gfd = p->fds[fd];
    if (gfd < 0) return -1;
    p->fds[fd] = -1;
    return vfs_close(gfd);
}

int fd_dup2(int oldfd, int newfd)
{
    struct proc *p;
    int gfd;
    if (oldfd < 0 || oldfd >= MAX_FDS) return -1;
    if (newfd < 0 || newfd >= MAX_FDS) return -1;
    p = proc_current();
    if (!p) return -1;
    gfd = p->fds[oldfd];
    if (gfd < 0) return -1;

    /* Close newfd if it's open */
    if (p->fds[newfd] >= 0)
    {
        vfs_close(p->fds[newfd]);
    }

    p->fds[newfd] = gfd;
    file_table[gfd].refcount++;
    return newfd;
}

void fd_init_stdio(void)
{
    struct proc *p;
    int tty_gfd;
    int i;

    p = proc_current();
    if (!p) return;

    /* Clear all fds */
    for (i = 0; i < MAX_FDS; i++)
        p->fds[i] = -1;

    /* Open /dev/tty for stdin, stdout, stderr */
    tty_gfd = vfs_open("/dev/tty", O_RDWR);
    if (tty_gfd >= 0)
    {
        p->fds[0] = tty_gfd;                   /* stdin */
        file_table[tty_gfd].refcount++;
        p->fds[1] = tty_gfd;                   /* stdout */
        file_table[tty_gfd].refcount++;
        p->fds[2] = tty_gfd;                   /* stderr */
        /* refcount is already 1 from open + 2 increments = 3 */
    }
}

void fd_inherit(struct proc *child, struct proc *parent)
{
    int i;
    int gfd;
    /* Inherit fds 0-2 (stdin/stdout/stderr) */
    for (i = 0; i < MAX_FDS; i++)
        child->fds[i] = -1;

    for (i = 0; i < 3; i++)
    {
        gfd = parent->fds[i];
        if (gfd >= 0)
        {
            child->fds[i] = gfd;
            file_table[gfd].refcount++;
        }
    }
}

void fd_close_all(void)
{
    struct proc *p;
    int i;
    p = proc_current();
    if (!p) return;
    for (i = 0; i < MAX_FDS; i++)
    {
        if (p->fds[i] >= 0)
        {
            vfs_close(p->fds[i]);
            p->fds[i] = -1;
        }
    }
}

void vfs_close_orphans(void)
{
    int i;
    /* Force-close any VFS entries except gfd 0 (kernel tty).
     * After a program exits, only the kernel's tty should remain open.
     * This catches leaked entries where brfs_close_all closed the BRFS
     * handle but the VFS entry's refcount was never decremented. */
    for (i = 1; i < MAX_OPEN_FILES; i++)
    {
        if (file_table[i].refcount > 0)
        {
            file_table[i].refcount = 1;
            vfs_close(i);
        }
    }
}
