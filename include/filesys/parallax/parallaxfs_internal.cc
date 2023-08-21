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

static struct __using("filesys/parallax/FlashFileSystem_16MB_eh.spin2") FlashFS;

// convert errors from Parallax codes to our codes
int ConvertError(int e) {
    switch (e) {
    case FlashFS.eInvalidHandle:
        return EBADF;
    case FlashFS.eNoHandle:
        return EMFILE;
    case FlashFS.eFileNotFound:
        return ENOENT;
    case FlashFS.eDriveFull:
        return ENOSPC;
    case FlashFS.eFileWriting:
        return EACCES;
    case FlashFS.eFileOpen:
        return EBUSY;
    case FlashFS.eFileExists:
        return EEXIST;
    default:
        return EIO;
    }
}

//
// try to call a Spin function, handling aborts gracefully
//
typedef int (*func0)(void);
typedef int (*func1)(void *arg1);
typedef int (*func2)(void *arg1, void *arg2);

int Try0(func0 f) {
    int r = 0;
    __try {
        r = f();
    } __catch (int e) {
        r = -ConvertError(e);
    }
    return r;
}

int Try1(func1 f, void *arg) {
    int r = 0;
    __try {
        r = f(arg);
    } __catch (int e) {
        r = -ConvertError(e);
    }
    return r;
}

int Try2(func2 f, void *arg1, void *arg2) {
    int r = 0;
    __try {
        r = f(arg1, arg2);
    } __catch (int e) {
        r = -ConvertError(e);
    }
    return r;
}

int fs_init()
{
    int r;
    r = Try0(&FlashFS.Mount);
    return _seterror(r);
}

//
// VFS hooks
//
#include <sys/types.h>
#include <sys/vfs.h>

static int v_creat(vfs_file_t *fil, const char *pathname, mode_t mode)
{
    int r;
    r = Try1(&FlashFS.OpenWrite, (void *)pathname);
    if (r < 0) {
        return _seterror(-r);
    }
    fil->vfsdata = (void *)r;  // flashfs handle
    return 0;
}

static int v_close(vfs_file_t *fil)
{
    int r = 0;
    r = Try1(&FlashFS.Close, fil->vfsdata);
    if (r < 0) {
        return _seterror(-r);
    }
    return 0;
}

static int v_opendir(DIR *dir, const char *name)
{
#ifdef _DEBUG_PFS
    __builtin_printf("v_opendir\n");
    FlashFS.DumpData();
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
    
    memset(buf, 0, sizeof(buf));
#ifdef _DEBUG_PFS
    __builtin_printf("v_readdir: handle=%d\n", handle);
#endif    
    FlashFS.Directory(&dir->vfsdata, buf, &size);
#ifdef _DEBUG_PFS
    _waitms(100);
    __builtin_printf(" ...buf=[%s] size=%d\n", buf, size);
#endif    
    if (buf[0] == 0) return -1; // EOF

    strcpy(ent->d_name, buf);
    ent->d_type = DT_REG; // all files are regular
    ent->d_size = Try1(&FlashFS.SizeOf, buf);
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
        if (!FlashFS.Exists(name)) {
            return _seterror(ENOENT);
        }
        fsize = Try1(&FlashFS.SizeOf, (void *)name);
    }
    mode |= (S_IRUSR | S_IRGRP | S_IROTH);
    mode |= (S_IWUSR | S_IWGRP | S_IWOTH);
    buf->st_mode = mode;
    buf->st_nlink = 1;
    buf->st_size = fsize;
    buf->st_blksize = 4096;
    buf->st_blocks = (fsize + 4095) / 4096;
    buf->st_atime = buf->st_mtime = buf->st_ctime = 0;
    return r;
}

static ssize_t v_read(vfs_file_t *fil, void *buf_p, size_t siz)
{
    int handle = (int)fil->vfsdata;
    char *buf = (char *)buf_p;
    int r;
    int c;
    
    r = 0;
    while (siz > 0) {
        c = FlashFS.ByteRead(handle);
        if (c < 0) break;
        *buf++ = c;
        ++r;
        --siz;
    }
    if (r == 0) {
        fil->state |= _VFS_STATE_EOF;
    }
    return r;
}
static ssize_t v_write(vfs_file_t *fil, void *buf_p, size_t siz)
{
    int handle = (int)fil->vfsdata;
    char *buf = (char *)buf_p;
    int r;
    int c;
    
    r = 0;
    while (siz > 0) {
        c = *buf++;
        __try {
            FlashFS.ByteWrite(handle, c);
            r++;
            --siz;
        } __catch(int e) {
            siz = 0;
        }
    }
    if (r == 0) {
        fil->state |= _VFS_STATE_EOF;
    }
    return r;
}

static off_t v_lseek(vfs_file_t *fil, off_t offset, int whence)
{
    // not supported yet
    return _seterror(ENOSEEK);
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
    r = Try1(&FlashFS.Delete, (void *)name);
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
    r = Try2(&FlashFS.Rename, (void *)oldname, (void *)newname);
    if (r < 0) {
        return _seterror(ENOENT);
    }
    return 0;
}

static int v_open(vfs_file_t *fil, const char *name, int flags)
{
  int handle = -1;
  int mode;
  
#ifdef _DEBUG_PFS
  __builtin_printf("pfs v_open\n");
#endif

  // check for read or write
  mode = flags & O_ACCMODE;
  switch (mode) {
  case O_RDONLY:
      handle = Try1(&FlashFS.OpenRead, (void *)name);
      break;
  case O_WRONLY:
      handle = Try1(&FlashFS.OpenWrite, (void *)name);
      break;
  default:
#ifdef _DEBUG_PFS
      __builtin_printf("pfs: invalid mode for open\n");
#endif      
      return _seterror(EINVAL);
  }

  if (handle < 0) {
#ifdef _DEBUG_PFS
      __builtin_printf("pfs: bad handle: %d\n", handle);
#endif      
      return _seterror(-handle);
  }
  fil->vfsdata = (void *)handle;
  return 0;
}

static struct vfs parallax_vfs =
{
    &v_close,
    &v_read,
    &v_write,
    &v_lseek,
    &v_ioctl,
    0, /* no flush function */
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
    0, /* deinit */
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
