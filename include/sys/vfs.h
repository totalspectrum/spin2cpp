#ifndef _SYS_VFS_H
#define _SYS_VFS_H

#include <sys/types.h>
#include <dirent.h>

typedef DIR vfs_dir_t;

#pragma once

struct vfs {
    // first 8 entries are sufficient to describe a read/write device
    int (*close)(vfs_file_t *fil);
    ssize_t (*read)(vfs_file_t *fil, void *buf, size_t siz);
    ssize_t (*write)(vfs_file_t *fil, const void *buf, size_t siz);
    off_t (*lseek)(vfs_file_t *fil, off_t offset, int whence);
    int   (*ioctl)(vfs_file_t *fil, unsigned long req, void *argp);
    int (*flush)(vfs_file_t *fil);
    void *vfs_data;   /* data needed for I/O */
    void *reserved;
    
    int (*open)(vfs_file_t *fil, const char *name, int flags);
    int (*creat)(vfs_file_t *fil, const char *pathname, mode_t mode);

    int (*opendir)(vfs_dir_t *dir, const char *name);
    int (*closedir)(vfs_dir_t *dir);
    int (*readdir)(vfs_dir_t *dir, struct dirent *ent);
    int (*stat)(const char *name, struct stat *buf);

    int (*mkdir)(const char *name, mode_t mode);
    int (*rmdir)(const char *name);
    
    int (*remove)(const char *pathname);
    int (*rename)(const char *oldname, const char *newname);

    int (*init)(const char *mountname);
    int (*deinit)(const char *mountname);
};

typedef struct vfs vfs_t;

int _openraw(void *f, const char *name, unsigned flags, unsigned perm) _IMPL("libc/unix/posixio.c");
int _closeraw(void *f) _IMPL("libc/unix/posixio.c");

// in FlexC these have definitions built in to the system module,
// so no need to _IMPL them
struct vfs *_getrootvfs(void);
void _setrootvfs(struct vfs *);

struct vfs *_vfs_open_host(void) _IMPL("filesys/fs9p/fs9p_vfs.c");
struct vfs *_vfs_open_sdcard(void) _IMPL("filesys/fatfs/fatfs_vfs.c");
struct vfs *_vfs_open_sdcardx(int pclk = 61, int pss = 60, int pdi = 59, int pdo = 58) _IMPL("filesys/fatfs/fatfs_vfs.c");

/* block read/write device */
typedef struct block_device {
    /* functions for doing I/O */
    int (*blk_read)(void *dst, unsigned long flashAdr, unsigned long size);
    int (*blk_write)(void *src, unsigned long flashAdr);  // write one block
    int (*blk_erase)(unsigned long flashAdr);             // erase one block
    int (*blk_sync)(void);

    /* buffers for the I/O */
    char *read_cache;
    char *write_cache;
    char *lookahead_cache;
} _BlockDevice;

/* structure for configuring a littlefs flash file system */
struct littlefs_flash_config {
    unsigned page_size;      // size of programming block, typically 256
    unsigned erase_size;     // size of erase blocks, typically 4K or 64K; must be a power of 2 and multiple of page_size
    unsigned offset;         // base address within flash, must be a multiple of erase_size
    unsigned used_size;      // size to be used within flash, must be a multiple of erase_size
    _BlockDevice *dev;       // device to use for I/O (NULL for default SPI flash)
    unsigned long long pinmask;        // pins used by device (0 for default)
    unsigned reserved;    // reserved for future use (pins and whatnot)
};
struct vfs *_vfs_open_littlefs_flash(int do_format = 1, struct littlefs_flash_config *cfg = 0) _IMPL("filesys/littlefs/lfs_spi_vfs.c");
int _mkfs_littlefs_flash(struct littlefs_flash_config *cfg = 0) _IMPL("filesys/littlefs/lfs_spi_vfs.c");

/* generic file buffer code */
/* put a "struct _default_buffer" at the start of your vfsdata to use the
 * default versions of putc and getc
 */
#ifdef __P2__
#define _DEFAULT_BUFSIZ 1024
#else
#define _DEFAULT_BUFSIZ 128
#endif

struct _default_buffer {
    int cnt;
    unsigned char *ptr;
    unsigned flags;
#define _BUF_FLAGS_READING (0x01)
#define _BUF_FLAGS_WRITING (0x02)    
    unsigned char buf[_DEFAULT_BUFSIZ];
};

int __default_getc(vfs_file_t *f) _IMPL("libc/unix/bufio.c");
int __default_putc(int c, vfs_file_t *f) _IMPL("libc/unix/bufio.c");
int __default_putc_terminal(int c, vfs_file_t *f) _IMPL("libc/unix/bufio.c");
int __default_flush(vfs_file_t *f) _IMPL("libc/unix/bufio.c");

int mount(const char *user_name, void *v) _IMPL("libc/unix/mount.c");
int umount(const char *user_name) _IMPL("libc/unix/mount.c");

/* directory functions */
char *__getfilebuffer();
struct vfs *__getvfsforfile(char *fullname, const char *orig_name, char *full_path);

#endif
