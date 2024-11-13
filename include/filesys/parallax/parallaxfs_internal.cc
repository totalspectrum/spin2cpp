//
// simple test program for 9p access to host files
// Written by Eric R. Smith, Total Spectrum Software Inc.
// Released into the public domain.
//

//#define _DEBUG_PFS

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/limits.h>

#ifndef PFS_MAX_FILES_OPEN
#define PFS_MAX_FILES_OPEN 2
#endif

typedef struct pfsfile {
    struct _default_buffer b;
    int32_t handle;
    int32_t offset;
    int32_t file_size;
} pfs_file;
    
static struct __using("flash_fs.spin2", MAX_FILES_OPEN=PFS_MAX_FILES_OPEN) FlashFS;

// convert errors from Parallax codes to our codes
static int ConvertError(int e) {
    switch (e) {
    case 0:
        return 0;
    case FlashFS.E_BAD_HANDLE:
        return EBADF;
    case FlashFS.E_NO_HANDLE:
        return EMFILE;
    case FlashFS.E_FILE_NOT_FOUND:
        return ENOENT;
    case FlashFS.E_DRIVE_FULL:
        return ENOSPC;
    case FlashFS.E_FILE_WRITING:
    case FlashFS.E_FILE_READING:
    case FlashFS.E_FILE_MODE:
        return EACCES;
    case FlashFS.E_FILE_OPEN:
        return EBUSY;
    case FlashFS.E_FILE_EXISTS:
        return EEXIST;
    default:
        return EIO;
    }
}

int fs_init()
{
    int r;
    r = FlashFS.Mount();
#ifdef _DEBUG_PFS
    __builtin_printf("..flash.mount returned %d\n", r);
#endif
    if (r) {
        return _seterror(ConvertError(r));
    }
    return 0;
}

//
// VFS hooks
//
#include <sys/types.h>
#include <sys/vfs.h>

static int v_creat(vfs_file_t *fil, const char *pathname, mode_t mode)
{
    int r;
    pfs_file *f = calloc(1, sizeof(*f));
    if (!f) {
        return _seterror(ENOMEM);
    }
    r = FlashFS.Open(pathname, 'w');
#ifdef _DEBUG_PFS
    __builtin_printf("..flash.open(%s, 'w') returned %d\n", pathname, r);
#endif
    if (r < 0) {
        free(f);
        return _seterror(ConvertError(r));
    }
#ifdef _DEBUG_PFS
    __builtin_printf("v_creat: handle %d\n", r);
#endif
    f->handle = r;
    f->file_size = 0;
    f->offset = 0;
    fil->vfsdata = (void *)f;
    return 0;
}

static int v_close(vfs_file_t *fil)
{
    int r = 0;
    pfs_file *f = fil->vfsdata;
    int handle = f->handle;

#ifdef _DEBUG_PFS
    __builtin_printf("v_close: handle %d\n", handle);
#endif    
    r = FlashFS.Close(handle);
#ifdef _DEBUG_PFS
    __builtin_printf("..flash.close returned %d\n", r);
#endif    
    if (r < 0) {
        return _seterror(ConvertError(r));
    }
    return 0;
}

static int v_opendir(DIR *dir, const char *name)
{
#ifdef _DEBUG_PFS
    __builtin_printf("v_opendir\n");
//    FlashFS.DumpData();
#endif    
    dir->vfsdata = 0;
    return 0;
}

static int v_closedir(DIR *dir)
{
    return 0;
}

static int v_readdir(DIR *dir, struct dirent *ent)
{
    char buf[128];
    int size;
    int handle = (int)dir->vfsdata;
    int r;
    
    memset(buf, 0, sizeof(buf));
#ifdef _DEBUG_PFS
    __builtin_printf("v_readdir: handle=%d\n", handle);
#endif    
    FlashFS.Directory(&dir->vfsdata, buf, &size);
#ifdef _DEBUG_PFS
    r = __builtin_printf("..flash.readdir returned %d\n", r);
#endif    
    if (buf[0] == 0) return -1; // EOF

#ifdef _DEBUG_PFS
    _waitms(100);
    __builtin_printf(" ...buf=[%s] size=%d\n", buf, size);
#endif    
    strcpy(ent->d_name, buf);
    ent->d_type = DT_REG; // all files are regular
    ent->d_size = FlashFS.file_size(buf);
    ent->d_mtime = 0; // time is not available
    return 0;
}

static int v_stat(const char *name, struct stat *buf)
{
    int r = 0;
    unsigned mode = 0;
    unsigned fsize;
#ifdef _DEBUG_PFS
    __builtin_printf("v_stat(%s)\n", name);
#endif    
    if (!name[0]) {
        // special case root directory
        fsize = 0;
        mode = S_IFDIR;
    } else {
        r = FlashFS.Exists(name);
#ifdef _DEBUG_PFS
        __builtin_printf("..flash.exists returned %d\n", r);
#endif            
        if (r == 0) {
            return _seterror(ENOENT);
        }
        fsize = FlashFS.file_size(name);
#ifdef _DEBUG_PFS
        __builtin_printf("..flash.file_size returned %d\n", fsize);
#endif            
    }
    mode |= (S_IRUSR | S_IRGRP | S_IROTH);
    mode |= (S_IWUSR | S_IWGRP | S_IWOTH);
    buf->st_mode = mode;
    buf->st_nlink = 1;
    buf->st_size = fsize;
    buf->st_blksize = 4096;
    buf->st_blocks = (fsize + 4095) / 4096;
    buf->st_atime = buf->st_mtime = buf->st_ctime = 0;
    return 0;
}

static ssize_t v_read(vfs_file_t *fil, void *buf_p, size_t siz)
{
    pfs_file *f = fil->vfsdata;
    int handle = f->handle;
    char *buf = (char *)buf_p;
    int r;
    int c;

#ifdef _DEBUG_PFS
    __builtin_printf("v_read(handle=%d siz=%d)...", handle, siz);
#endif    
    r = FlashFS.read(handle, buf, siz);
#ifdef _DEBUG_PFS
    __builtin_printf("..flash.read(%d, $%x, %d) returned %d\n", handle, (unsigned)buf, siz, r);
#endif
    if (r == 0) {
        r = FlashFS.error();
        if (r == FlashFS.E_END_OF_FILE) {
            r = 0;
            fil->state |= _VFS_STATE_EOF;
        } else {
            fil->state |= _VFS_STATE_ERR;
        }
    }
    if (r > 0) {
        f->offset += r;
        if (f->offset > f->file_size) {
            // unlikely, but maybe some other handle was used to extend the file??
            f->file_size = f->offset;
        }
    }
#ifdef _DEBUG_PFS
    __builtin_printf("returning %d\n", r);
#endif    
    return r;
}
static ssize_t v_write(vfs_file_t *fil, void *buf_p, size_t siz)
{
    pfs_file *f = fil->vfsdata;
    int handle = f->handle;
    char *buf = (char *)buf_p;
    int r;
    int c;
    
#ifdef _DEBUG_PFS
    __builtin_printf("v_write(handle=%d siz=%d)...", handle, siz);
#endif    
    r = FlashFS.write(handle, buf_p, siz);
#ifdef _DEBUG_PFS
        __builtin_printf("..flash.write returned %d\n", r);
#endif            
    if (r == 0) {
        r = FlashFS.error();
        if (r == FlashFS.E_END_OF_FILE) {
            r = 0;
            fil->state |= _VFS_STATE_EOF;
        } else {
            fil->state |= _VFS_STATE_ERR;
        }
    }
    if (r > 0) {
        f->offset += r;
        if (f->offset > f->file_size) {
            f->file_size = f->offset;
        }
    }
#ifdef _DEBUG_PFS
    __builtin_printf("returning %d\n", r);
#endif    
    return r;
}

static off_t v_lseek(vfs_file_t *fil, off_t offset, int whence)
{
    pfs_file *f = fil->vfsdata;
    int handle = f->handle;
    int32_t where;
    int r;

#ifdef _DEBUG_PFS
    __builtin_printf("v_lseek(%d, %ld, %d) current offset: %d file_size: %d\n", handle, (long)offset, whence, f->offset, f->file_size);
#endif    
    switch (whence) {
    case SEEK_SET:
        where = offset; break;
    case SEEK_CUR:
        where = f->offset + offset; break;
    case SEEK_END:
        where = f->file_size + offset; break;
    default:
        return _seterror(EINVAL);
    }
    if (where < 0 || where > f->file_size) {
        // technically seeking past end of file should extend the file,
        // but that's tricky and parallax FS doesn't support it
        return _seterror(EINVAL);
    }
    r = FlashFS.seek(handle, where, FlashFS.SK_FILE_START);
#ifdef _DEBUG_PFS
    __builtin_printf("..flash.seek( %d, %d, %d ) returned %d\n", handle, where, FlashFS.SK_FILE_START, r);
#endif    
    if (r < 0) {
        // error
        return _seterror(ConvertError(r));
    }
    f->offset = where;
    return where;
}

static int v_ioctl(vfs_file_t *fil, unsigned long req, void *argp)
{
    return _seterror(ENOSYS);
}

static int v_mkdir(const char *name, mode_t mode)
{
    return _seterror(ENOSYS);
}

static int v_remove(const char *name)
{
    int r;
    r = FlashFS.Delete(name);
#ifdef _DEBUG_PFS
    __builtin_printf("..flash.delete(%s) returned %d\n", name, r);
#endif            
    if (r < 0) {
        return _seterror(ENOENT);
    }
    return 0;
}

static int v_rmdir(const char *name)
{
    return _seterror(ENOSYS);
}

static int v_rename(const char *oldname, const char *newname)
{
    int r;
    r = FlashFS.Rename(oldname, newname);
#ifdef _DEBUG_PFS
    __builtin_printf("..flash.rename returned %d\n", r);
#endif            
    if (r < 0) {
        return _seterror(ENOENT);
    }
    return 0;
}

static int v_open(vfs_file_t *fil, const char *name, int flags)
{
    pfs_file *f;
    int handle = -1;
    int fsize = -1;
    int offset = 0;
    int mode;
  
#ifdef _DEBUG_PFS
    __builtin_printf("pfs v_open(%s)\n", name);
#endif
    f = calloc(1, sizeof(*f));
    if (!f) {
        return _seterror(ENOMEM);
    }
    // check for read or write
    mode = flags & O_ACCMODE;
    switch (mode) {
    case O_RDONLY:
        fsize = FlashFS.file_size(name);
#ifdef _DEBUG_PFS
        __builtin_printf("..flash.file_size returned %d\n", fsize);
#endif            
        handle = FlashFS.Open(name, 'r');
#ifdef _DEBUG_PFS
        __builtin_printf("..flash.open(%s, 'r') returned %d\n", name, handle);
#endif            
        break;
    case O_WRONLY:
        if (flags & O_APPEND) {
            fsize = FlashFS.file_size(name);
#ifdef _DEBUG_PFS
            __builtin_printf("..flash.file_size returned %d\n", fsize);
#endif            
            offset = fsize;
            handle = FlashFS.Open(name, 'a');
#ifdef _DEBUG_PFS
            __builtin_printf("..flash.open(%s, 'a') returned %d\n", name, handle);
#endif            
        } else {
            fsize = 0;
            handle = FlashFS.Open(name, 'w');
#ifdef _DEBUG_PFS
            __builtin_printf("..flash.open(%s, 'w') returned %d\n", name, handle);
#endif            
        }
        break;
    default:
#ifdef _DEBUG_PFS
        __builtin_printf("pfs: invalid mode for open\n");
#endif
        free(f);
        return _seterror(EINVAL);
    }

#ifdef _DEBUG_PFS
    __builtin_printf("...pfs returned handle %d\n", handle);
#endif  
    if (handle < 0) {
#ifdef _DEBUG_PFS
        __builtin_printf("pfs: bad handle: %d\n", handle);
#endif
        free(f);
        return _seterror(ConvertError(handle));
    }
    f->handle = handle;
    f->offset = offset;
    f->file_size = fsize;
    fil->vfsdata = (void *)f;
    return 0;
}

static int v_flush(vfs_file_t *fil)
{
    pfs_file *f = fil->vfsdata;
    int handle = f->handle;
    int r = 0;

    __default_flush(fil); // write out buffered data
    r = FlashFS.flush(handle);
#ifdef _DEBUG_PFS
    __builtin_printf("..flash.flush returned %d\n", r);
#endif            
    if (r < 0) {
        return _seterror(ConvertError(r));
    }
    return r;
}

/* WARNING: copy in parallaxfs_vfs.c */
enum {
    PFS_pclk = 61,
    PFS_pss = 60,
    PFS_pdi = 59,
    PFS_pdo = 58
};

/* deinitialize (unmount) */
static int v_deinit(const char *mountname)
{
    int r;
    unsigned long long pmask = (1ULL << PFS_pclk) | (1ULL << PFS_pss) | (1ULL << PFS_pdi) | (1ULL << PFS_pdo);

    r = FlashFS.unmount();
#ifdef _DEBUG_PFS
    __builtin_printf("flash.unmount() returned %d\n", r);
#endif    
    _freepins(pmask);
    return 0;
}

static struct vfs parallax_vfs =
{
    &v_close,
    &v_read,
    &v_write,
    &v_lseek,
    &v_ioctl,
    &v_flush,
    0, /* reserved1 */
    0, /* reserved2 */
    
    &v_open,
    &v_creat,
    &v_opendir,
    &v_closedir,
    &v_readdir,
    &v_stat,

    &v_mkdir,
    &v_rmdir,
    &v_remove,
    &v_rename,

    0, /* init */
    &v_deinit, /* deinit */

    0, /* getcf */
    0, /* putcf */
};

struct vfs *
get_vfs()
{
  struct vfs *v = &parallax_vfs;
#ifdef _DEBUG_PFS
  __builtin_printf("get_vfs: returning %x\n", (unsigned)v);
#endif  
  return v;
}

int
mkfs()
{
    FlashFS.Format();
    return 0;
}
