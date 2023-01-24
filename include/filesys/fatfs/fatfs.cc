/*
 * Interface to FatFs
 * Designed to work with FatFs R0.14 and later
 * This code is written by Eric R. Smith and placed in the
 * public domain. The original FatFs copyright appears
 * below; see also ff.c and similar files.
 */

#define DIR FFDIR
#define get_fattime _get_fattime
#define static

#include "ffunicode.c"
#include "ff.c"
#undef DIR
#undef static

/////////////////////////////////////////////////////////////////////////
// FlexC virtual file system glue code
/////////////////////////////////////////////////////////////////////////
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

unsigned int _get_fattime()
{
    time_t now;
    unsigned ffdate, fftime;
    struct tm date;
    
    now = time(0);
    gmtime_r(&now, &date);
    ffdate = ((date.tm_year - 80) << 9) | ((date.tm_mon + 1) << 5) | ((date.tm_mday));
    fftime = (date.tm_hour << 11) | (date.tm_min << 5) | (date.tm_sec);
    return (ffdate << 16) | fftime;
}

static time_t unixtime(unsigned int dosdate, unsigned int dostime)
{
    time_t t;
    struct tm date;
    
    date.tm_year = 80 + ((dosdate >> 9) & 0x7f);
    date.tm_mon = ((dosdate >> 5) & 0xf) - 1;
    date.tm_mday = (dosdate & 0x1f);
    date.tm_hour = (dostime >> 11) & 0x1f;
    date.tm_min = (dostime >> 5) & 0x3f;
    date.tm_sec = (dostime & 0x1f) << 1;

    t = mktime(&date);
#if 0
    __builtin_printf(" ... unixtime: year=%u month = %u day = %u t = %u\n", year, month, day, t);
#endif    
    return t;
}

typedef struct fat_file {
    struct _default_buffer b;
    FIL fil;
} FAT_FIL;

static int _set_dos_error(int derr)
{
    int r;
#ifdef _DEBUG_FATFS
    __builtin_printf("_set_dos_error(%d)\n", derr);
#endif    
    switch (derr) {
    case FR_OK:
        r = 0;
        break;
    case FR_NO_FILE:
    case FR_NO_PATH:
    case FR_INVALID_NAME:
        r = ENOENT;
        break;
    case FR_DENIED:
    case FR_WRITE_PROTECTED:
        r = EACCES;
        break;
    case FR_EXIST:
        r = EEXIST;
        break;
    case FR_NOT_ENOUGH_CORE:
        r = ENOMEM;
        break;
    case FR_INVALID_PARAMETER:
    case FR_INVALID_OBJECT:
    case FR_INVALID_DRIVE:
    case FR_NOT_ENABLED:
    case FR_NO_FILESYSTEM:
        r = EINVAL;
        break;
    case FR_TOO_MANY_OPEN_FILES:
        r = EMFILE;
        break;
        
    case FR_DISK_ERR:
    case FR_INT_ERR:
    case FR_NOT_READY:
    default:
        r = EIO;
        break;
    }
    return _seterror(r);
}

int v_creat(vfs_file_t *fil, const char *pathname, mode_t mode)
{
  int r;
  FAT_FIL *f = malloc(sizeof(*f));
  int fatmode;
  
  if (!f) {
      return _seterror(ENOMEM);
  }
  memset(f, 0, sizeof(*f));
  fatmode = FA_CREATE_NEW | FA_WRITE | FA_READ;
  r = f_open(&f->fil, pathname, fatmode);
#ifdef _DEBUG_FATFS
  __builtin_printf("v_create(%s) returned %d\n", pathname, r);
#endif  
  if (r) {
    free(f);
    return _set_dos_error(r);
  }
  fil->vfsdata = f;
  return 0;
}

int v_close(vfs_file_t *fil)
{
    int r;
    FAT_FIL *f = fil->vfsdata;
    r=f_close(&f->fil);
    free(f);
    return _set_dos_error(r);
}

int v_opendir(DIR *dir, const char *name)
{
    FFDIR *f = malloc(sizeof(*f));
    int r;

#ifdef _DEBUG_FATFS    
    __builtin_printf("v_opendir(%s) this=%x\n", name, __this);
#endif    
    if (!f) {
#ifdef _DEBUG_FATFS
      __builtin_printf("malloc failed\n");
#endif    
      return _seterror(ENOMEM);
    }
    r = f_opendir(f, name);
#ifdef _DEBUG_FATFS
    __builtin_printf("f_opendir returned %d\n", r);
#endif    
    
    if (r) {
        free(f);
        return _set_dos_error(r);
    }
    dir->vfsdata = f;
    return 0;
}

int v_closedir(DIR *dir)
{
    int r;
    FFDIR *f = dir->vfsdata;
    r = f_closedir(f);
    free(f);
    if (r) _set_dos_error(r);
    return r;
}

int v_readdir(DIR *dir, struct dirent *ent)
{
    FILINFO finfo;
    int r;

    r = f_readdir(dir->vfsdata, &finfo);
#ifdef _DEBUG_FATFS       
    __builtin_printf("readdir fs_read: %d\n", r);
#endif	
    if (r != 0) {
        return _set_dos_error(r); // error
    }
    if (finfo.fname[0] == 0) {
        return -1; // EOF
    }
#if FF_USE_LFN
    strncpy(ent->d_name, finfo.fname, _NAME_MAX-1);
    ent->d_name[_NAME_MAX-1] = 0;
#else
    strcpy(ent->d_name, finfo.fname);
#endif
    if (finfo.fattrib & AM_DIR) {
        ent->d_type = DT_DIR;
    } else {
        ent->d_type = DT_REG;
    }
    ent->d_size = finfo.fsize;
    ent->d_mtime = unixtime(finfo.fdate, finfo.ftime);
    return 0;
}

int v_stat(const char *name, struct stat *buf)
{
    int r;
    FILINFO finfo;
    unsigned mode;
#ifdef _DEBUG_FATFS
    __builtin_printf("v_stat(%s)\n", name);
#endif
    memset(buf, 0, sizeof(*buf));
    if (name[0] == 0 || (name[0] == '.' && name[1] == 0)) {
        /* root directory */
        finfo.fattrib = AM_DIR;
        r = 0;
    } else {
        r = f_stat(name, &finfo);
    }
    if (r != 0) {
        return _set_dos_error(r);
    }
    mode = S_IRUSR | S_IRGRP | S_IROTH;
    if (finfo.fattrib & AM_RDO) {
        mode |= S_IWUSR | S_IWGRP | S_IWOTH;
    }
    if (finfo.fattrib & AM_DIR) {
        mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
    }
    buf->st_mode = mode;
    buf->st_nlink = 1;
    buf->st_size = finfo.fsize;
    buf->st_blksize = 512;
    buf->st_blocks = buf->st_size / 512;
    buf->st_atime = buf->st_mtime = buf->st_ctime = unixtime(finfo.fdate, finfo.ftime);
#ifdef _DEBUG_FATFS
    __builtin_printf("v_stat returning %d mode=0x%x\n", r, buf->st_mode);
#endif
    return r;
}

ssize_t v_read(vfs_file_t *fil, void *buf, size_t siz)
{
    FAT_FIL *f = fil->vfsdata;
    int r;
    UINT x;
    
    if (!f) {
        return _seterror(EBADF);
    }
#ifdef _DEBUG_FATFS    
    __builtin_printf("v_read: fs_read of %u bytes:", siz);
#endif    
    r = f_read(&f->fil, buf, siz, &x);
#ifdef _DEBUG_FATFS
    __builtin_printf(" ...returned %d\n", r);
#endif    
    if (r != 0) {
        fil->state |= _VFS_STATE_ERR;
        return _set_dos_error(r);
    }
    if (x == 0) {
        fil->state |= _VFS_STATE_EOF;
    }
    return x;
}
ssize_t v_write(vfs_file_t *fil, void *buf, size_t siz)
{
    FAT_FIL *f = fil->vfsdata;
    int r;
    UINT x;
    if (!f) {
        return _seterror(EBADF);
    }
#if FF_FS_READONLY
    return _seterror(EACCES);
#else
#ifdef _DEBUG_FATFS    
    __builtin_printf("v_write: f_write %d bytes:", siz);
#endif    
    r = f_write(&f->fil, buf, siz, &x);
#ifdef _DEBUG_FATFS
    __builtin_printf("returned %d\n", r);
#endif    
    if (r != 0) {
        fil->state |= _VFS_STATE_ERR;
        return _set_dos_error(r);
    }
    return x;
#endif
}
off_t v_lseek(vfs_file_t *fil, off_t offset, int whence)
{
    FAT_FIL *vf = fil->vfsdata;
    FIL *f = &vf->fil;
    int result;
    
    if (!f) {
        return _seterror(EBADF);
    }
#ifdef DEBUG
    __builtin_printf("v_lseek(%d, %d) ", offset, whence);
#endif    
    if (whence == SEEK_SET) {
        /* offset is OK */
    } else if (whence == SEEK_CUR) {
        offset += f->fptr;
    } else {
        offset += f->obj.objsize;
    }
    result = f_lseek(f, offset);
#ifdef DEBUG
    __builtin_printf("result=%d\n", result);
#endif
    if (result) {
        return _set_dos_error(result);
    }
    return offset;
}

int v_ioctl(vfs_file_t *fil, unsigned long req, void *argp)
{
    return _seterror(EINVAL);
}

int v_mkdir(const char *name, mode_t mode)
{
#if FF_FS_READONLY
    return _seterror(EACCES);
#else
    int r;

    r = f_mkdir(name);
    return _set_dos_error(r);
#endif
}

int v_remove(const char *name)
{
#if FF_FS_READONLY
    return _seterror(EACCES);
#else
    int r;

    r = f_unlink(name);
    return _set_dos_error(r);
#endif
}

int v_rmdir(const char *name)
{
#if FF_FS_READONLY
    return _seterror(EACCES);
#else
    int r;

    r = f_unlink(name);
    return _set_dos_error(r);
#endif
}

int v_rename(const char *old, const char *new)
{
#if FF_FS_READONLY
    return _seterror(EACCES);
#else
    int r = f_rename(old, new);
    return _set_dos_error(r);
#endif
}
 
int v_open(vfs_file_t *fil, const char *name, int flags)
{
  int r;
  FAT_FIL *f = malloc(sizeof(*f));
  unsigned fs_flags;

  if (!f) {
      return _seterror(ENOMEM);
  }
  memset(f, 0, sizeof(*f));
  switch (flags & 3) {
  case O_RDONLY:
      fs_flags = FA_READ;
      break;
  case O_WRONLY:
      fs_flags = FA_WRITE;
      break;
  default:
      fs_flags = FA_READ | FA_WRITE;
      break;
  }
  
  if (flags & O_TRUNC) {
      fs_flags |= FA_CREATE_ALWAYS | FA_OPEN_ALWAYS;
  } else if (flags & O_APPEND) {
      fs_flags |= FA_OPEN_APPEND;
  } else {
    //fs_flags |= FA_OPEN_ALWAYS; // no, FA_OPEN_ALWAYS creates a file
  }
  r = f_open(&f->fil, name, fs_flags);
  if (r) {
    free(f);
#if defined(_DEBUG_FATFS) && defined(__FLEXC__)
    __builtin_printf("  f_open returned %d\n", r);
#endif                        
    return _set_dos_error(r);
  }
  fil->vfsdata = f;
  return 0;
}

/* initialize (do first mount) */
/* for now this is a dummy function, but eventually some of the work
 * in _vfs_open_sdcardx could be done here
 */
int v_init(const char *mountname)
{
    return 0;
}

/* deinitialize (unmount) */
int v_deinit(const char *mountname)
{
    int r = f_mount(0, "", 0);
#if defined(_DEBUG_FATFS) && defined(__FLEXC__)
    __builtin_printf("  deinit: f_mount returned %d\n", r);
#endif                        
    return 0;
}

static struct vfs fat_vfs =
{
    &v_close,
    &v_read,
    &v_write,
    &v_lseek,
    
    &v_ioctl,
    0, /* no flush function */
    0, /* vfs_data */
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

    &v_init, /* init */
    &v_deinit, /* deinit */
};

struct vfs *
get_vfs(void *ptr)
{
    struct vfs *v = _gc_alloc_managed(sizeof(*v));

    v->close = &v_close;
    v->read = &v_read;
    v->write = &v_write;
    v->lseek = &v_lseek;
    v->ioctl = &v_ioctl;
    v->flush = 0;  // no flush function
    v->vfs_data = ptr;
    v->reserved = 0;

    v->open = &v_open;
    v->creat = &v_creat;
    v->opendir = &v_opendir;
    v->closedir = &v_closedir;
    v->readdir = &v_readdir;
    v->stat = &v_stat;

    v->mkdir = &v_mkdir;
    v->rmdir = &v_rmdir;

    v->remove = &v_remove;
    v->rename = &v_rename;

    v->init = &v_init;
    v->deinit = &v_deinit;
    
    return v;
}
