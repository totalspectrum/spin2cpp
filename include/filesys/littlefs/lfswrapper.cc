#include <unistd.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "lfs.c"

#define SPI_PROG_SIZE          256

typedef struct __using("filesys/littlefs/SpiFlash.spin2") _SpiFlash;

typedef struct _buffered_lfs_file {
    struct _default_buffer b;
    lfs_file_t       fd;
} BufferedLfsFile;


static vfs_file_t *Default_SPI_Init(unsigned offset, unsigned used_size, unsigned erase_size)
{
    static _SpiFlash spi;
    static vfs_file_t *handle;

    spi.Init(offset, used_size, erase_size);

    handle = _get_vfs_file_handle();
    if (!handle) {
        _seterror(ENFILE);
        return 0;
    }
    handle->flags = O_RDWR;
    handle->bufmode = _IONBF;
    handle->state = _VFS_STATE_INUSE | _VFS_STATE_WROK | _VFS_STATE_RDOK;
    
    handle->read = &spi.v_read;
    handle->write = &spi.v_write;
    handle->lseek = (void *)&spi.v_lseek; /* cast to fix a warning */
    handle->ioctl = &spi.v_ioctl;
    handle->flush = &spi.v_flush;
#ifdef _DEBUG_LFS
    __builtin_printf("Default_SPI_Init gives: %x\n", handle);
#endif
    return handle;
}


static int _flash_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    vfs_file_t *handle = (vfs_file_t *)cfg->context;
    unsigned long flashAdr = block * cfg->block_size + off;
    
    handle->lseek(handle, (off_t)flashAdr, 0);
    handle->read(handle, buffer, size);
    return 0;
}

static int _flash_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer_orig, lfs_size_t size) {
    vfs_file_t *handle = (vfs_file_t *)cfg->context;
    unsigned flashAdr = block * cfg->block_size + off;
    char *buffer = buffer_orig;
    unsigned PAGE_SIZE = cfg->prog_size;
    unsigned PAGE_MASK = PAGE_SIZE-1; // assumes PAGE_SIZE is a power of 2

#ifdef _DEBUG_LFS
    __builtin_printf(" *** flash_prog: block=%d off=%x size=%x\n", block, off, size);
#endif        
    // make sure size is a page multiple
    if ( size % PAGE_SIZE ) {
#ifdef _DEBUG_LFS
    __builtin_printf(" *** flash_prog: bad size EINVAL\n");
#endif        
        return -EINVAL;
    }
#ifdef _DEBUG_LFS
    __builtin_printf(" *** flash_prog: write %u bytes to %u\n", size, flashAdr);
#endif        
    handle->lseek(handle, (off_t)flashAdr, 0);
    handle->write(handle, buffer, size);
    return 0;
}

static int _flash_erase(const struct lfs_config *cfg, lfs_block_t block) {
    vfs_file_t *handle = (vfs_file_t *)cfg->context;
    unsigned long flashAdr = block * cfg->block_size;
    unsigned size = cfg->block_size;
    
#ifdef _DEBUG_LFS
    __builtin_printf(" *** flash_erase: block=0x%x\n", block);
#endif        
    // check if erase is valid
    if (block >= cfg->block_count) {
#ifdef _DEBUG_LFS
    __builtin_printf(" *** flash_erase: bad address (block_count=0x%x)\n", cfg->block_count);
#endif        
        return -1;
    }
    struct _blkio_info block_info = {
        .addr = flashAdr,
        .size = size
    };
    // try erasing first
#ifdef _DEBUG_LFS
    __builtin_printf(" ... handle->ioctl: %x %x\n", handle, handle->ioctl);
#endif    
    if (handle->ioctl(handle, BLKIOCTLERASE, &block_info) == 0) {
#ifdef _DEBUG_LFS
        __builtin_printf(" *** flash_erase: fast erase worked\n");
#endif        
        return 0;
    }
    // write FF here
#ifdef _DEBUG_LFS
    __builtin_printf(" *** flash_erase: slow erase path\n");
#endif        
    unsigned char buf[256];
    unsigned chunk;
    memset(buf, 0xff, sizeof(buf));
    handle->lseek(handle, (off_t)flashAdr, 0);
    while (size > 0) {
        chunk = (size < sizeof(buf)) ? size : sizeof(buf);
        handle->write(handle, buf, chunk);
        size -= chunk;
    }
    return 0;
}

static int _flash_sync(const struct lfs_config *cfg) {
    vfs_file_t *handle = (vfs_file_t *)cfg->context;
    return handle->flush(handle);
}

static int _flash_create(struct lfs_config *cfg, struct littlefs_flash_config *flashcfg)
{
    vfs_file_t *handle = flashcfg->handle;
    static bool default_cache_used = false;
    static char read_cache[SPI_PROG_SIZE];
    static char prog_cache[SPI_PROG_SIZE];
    static char lookahead_cache[SPI_PROG_SIZE];

    if (!handle) {
        handle = Default_SPI_Init(flashcfg->offset, flashcfg->used_size, flashcfg->erase_size);
    }
#ifdef _DEBUG_LFS
    __builtin_printf("_flash_create: handle to use=%x\n", handle);
#endif    
    cfg->context = (void *)handle;
    
    if (flashcfg->page_size != SPI_PROG_SIZE) {
        return -EINVAL;
    }
    if (flashcfg->used_size % flashcfg->erase_size != 0) {
        return -EINVAL;
    }
    if (flashcfg->offset % flashcfg->erase_size != 0) {
        return -EINVAL;
    }
    
    // set up flash properties
    cfg->read_size = SPI_PROG_SIZE;
    cfg->prog_size = SPI_PROG_SIZE;
    cfg->block_size = flashcfg->erase_size;
    cfg->block_count = flashcfg->used_size / flashcfg->erase_size;
    cfg->cache_size = SPI_PROG_SIZE;
    cfg->lookahead_size = SPI_PROG_SIZE;
    cfg->block_cycles = 400;

    // buffers
    if (default_cache_used) {
        // dynamically allocate more memory
        int blksize = SPI_PROG_SIZE;
        cfg->read_buffer = malloc(blksize);
        cfg->prog_buffer = malloc(blksize);
        cfg->lookahead_buffer = malloc(blksize);
    } else {
        cfg->read_buffer = read_cache;
        cfg->prog_buffer = prog_cache;
        cfg->lookahead_buffer = lookahead_cache;
        default_cache_used = true;
    }
    // set up block device operations
    cfg->read = _flash_read;
    cfg->prog = _flash_prog;
    cfg->erase = _flash_erase;
    cfg->sync = _flash_sync;

    return 0;
}

///////////////////////////////////////////////////////////////////////
// FlexC VFS code
///////////////////////////////////////////////////////////////////////

char lfs_in_use;
lfs_t lfs;
struct lfs_config lfs_cfg;

static int _set_lfs_error(int lerr)
{
    int r;
    switch (lerr) {
    case LFS_ERR_OK:
        r = 0;
        break;
    case LFS_ERR_IO:
    case LFS_ERR_CORRUPT:
        r = EIO;
        break;
    case LFS_ERR_NOENT:
        r = ENOENT;
        break;
    case LFS_ERR_EXIST:
        r = EEXIST;
        break;
    case LFS_ERR_NOTDIR:
        r = ENOTDIR;
        break;
    case LFS_ERR_ISDIR:
        r = EISDIR;
        break;
    case LFS_ERR_NOTEMPTY:
        r = ENOTEMPTY;
        break;
    case LFS_ERR_BADF:
        r = EBADF;
        break;
    case LFS_ERR_INVAL:
        r = EINVAL;
        break;
    case LFS_ERR_NOSPC:
        r = ENOSPC;
        break;
    case LFS_ERR_NOMEM:
        r = ENOMEM;
        break;
    case LFS_ERR_NAMETOOLONG:
        r = ENAMETOOLONG;
        break;
    default:
        r = EIO;
        break;
    }
    return _seterror(r);
}

static int v_open(vfs_file_t *fil, const char *name, int flags)
{
  int r;
  BufferedLfsFile *f = malloc(sizeof(*f));
  unsigned fs_flags;

  if (!f) {
#ifdef _DEBUG_LFS
      __builtin_printf("lfs_open: ENOMEM\n");
#endif  
      return _seterror(ENOMEM);
  }
  memset(f, 0, sizeof(*f));
  switch (flags & 3) {
  case O_RDONLY:
      fs_flags = LFS_O_RDONLY;
      break;
  case O_WRONLY:
      fs_flags = LFS_O_WRONLY;
      break;
  default:
      fs_flags = LFS_O_RDWR;
      break;
  }
  
  if (flags & O_TRUNC) {
      fs_flags |= LFS_O_TRUNC;
  }
  if (flags & O_APPEND) {
      fs_flags |= LFS_O_APPEND;
  }
  if (flags & O_CREAT) {
      fs_flags |= LFS_O_CREAT;
  }
#ifdef _DEBUG_LFS
  __builtin_printf("calling lfs_open(%s, 0x%x)", name, fs_flags);
#endif  
  r = lfs_file_open(&lfs, &f->fd, name, fs_flags);
#if defined(_DEBUG_LFS) && defined(__FLEXC__)
  __builtin_printf("  lfs_open returned %d\n", r);
#endif                        
  if (r) {
    free(f);
    return _set_lfs_error(r);
  }
  fil->vfsdata = f;
  return 0;
}

static int v_creat(vfs_file_t *fil, const char *pathname, mode_t mode)
{
#ifdef _DEBUG_LFS
    __builtin_printf(" v_creat(%s)\n", pathname);
#endif    
    return v_open(fil, pathname, O_CREAT|O_WRONLY|O_TRUNC);
}

static int v_flush(vfs_file_t *fil)
{
    BufferedLfsFile *f = fil->vfsdata;

    __default_flush(fil); // write out buffered data

#ifdef _DEBUG_LFS
    __builtin_printf("v_flush...");
#endif    
    int r = lfs_file_sync(&lfs, &f->fd);
#ifdef _DEBUG_LFS
    __builtin_printf(" returned %d\n", r);
#endif    
    return 0;
}

static int v_close(vfs_file_t *fil)
{
    int r;
    BufferedLfsFile *f = fil->vfsdata;

    r = lfs_file_close(&lfs, &f->fd);
#ifdef _DEBUG_LFS
    __builtin_printf("lfs_file_close returned %d\n", r);
#endif    
    free(f);
    return _set_lfs_error(r);
}

static ssize_t v_read(vfs_file_t *fil, void *buf, size_t siz)
{
    BufferedLfsFile *f = fil->vfsdata;
    int r;

#ifdef _DEBUG_LFS    
    __builtin_printf("v_read: f_read %d bytes:", siz);
#endif    
    if (!f) {
#ifdef _DEBUG_FS
        __builtin_printf(" EBADF\n");
#endif        
        return _seterror(EBADF);
    }
    r = lfs_file_read(&lfs, &f->fd, buf, siz);
#ifdef _DEBUG_LFS
    __builtin_printf("returned %d\n", r);
#endif    
    if (r < 0) {
        fil->state |= _VFS_STATE_ERR;
        return _set_lfs_error(r);
    }
    return r;
}

static ssize_t v_write(vfs_file_t *fil, void *buf, size_t siz)
{
    BufferedLfsFile *f = fil->vfsdata;
    int r;

#ifdef _DEBUG_LFS    
    __builtin_printf("v_write: f_write %d bytes:", siz);
#endif    
    if (!f) {
#ifdef _DEBUG_LFS
        __builtin_printf("EBADF\n");
#endif        
        return _seterror(EBADF);
    }
    r = lfs_file_write(&lfs, &f->fd, buf, siz);
#ifdef _DEBUG_LFS
    __builtin_printf("returned %d\n", r);
#endif    
    if (r < 0) {
        fil->state |= _VFS_STATE_ERR;
        return _set_lfs_error(r);
    }
    //lfs_file_sync(&lfs, &f->fd);
    return r;
}

static off_t v_lseek(vfs_file_t *fil, off_t offset, int whence)
{
    BufferedLfsFile *f = fil->vfsdata;
    int r;
    
#ifdef _DEBUG_LFS
    __builtin_printf("v_lseek(%ld, %d) ", (long)offset, whence);
#endif
    if (!f) {
        return _seterror(EBADF);
    }
    r = lfs_file_seek(&lfs, &f->fd, (lfs_soff_t)offset, whence);
#ifdef _DEBUG_LFS
    __builtin_printf("result=%d\n", r);
#endif
    if (r < 0) {
        return _set_lfs_error(r);
    }
    return r;
}

static int v_ioctl(vfs_file_t *fil, unsigned long req, void *argp)
{
#ifdef _DEBUG_LFS
    __builtin_printf("v_ioctl\n");
#endif    
    return _seterror(EINVAL);
}

static int v_remove(const char *name)
{
    int r = lfs_remove(&lfs, name);
#ifdef _DEBUG_LFS
    __builtin_printf("lfs_remove(%s) returned %d\n", name, r);
#endif    
    return _set_lfs_error(r);
}

static int v_rename(const char *oldpath, const char *newpath)
{
    int r = lfs_rename(&lfs, oldpath, newpath);
#ifdef _DEBUG_LFS
    __builtin_printf("lfs_rename(%s, %s) returned %d\n", oldpath, newpath, r);
#endif    
    return _set_lfs_error(r);
}

static int v_mkdir(const char *name, mode_t mode)
{
    int r = lfs_mkdir(&lfs, name);
#ifdef _DEBUG_LFS
    __builtin_printf("lfs_mkdir(%s) returned %d\n", name, r);
#endif    
    return _set_lfs_error(r);
}

static int v_rmdir(const char *name)
{
    int r = lfs_remove(&lfs, name);
#ifdef _DEBUG_LFS
    __builtin_printf("lfs_rmdir(%s) returned %d\n", name, r);
#endif    
    return _set_lfs_error(r);
}

static int v_stat(const char *name, struct stat *buf)
{
    int r;
    struct lfs_info finfo;
    unsigned mode;
#ifdef _DEBUG_LFS
    __builtin_printf("v_stat(%s)\n", name);
#endif
    memset(buf, 0, sizeof(*buf));

    r = lfs_stat(&lfs, name, &finfo);
    if (r < 0) {
#ifdef _DEBUG_LFS
        __builtin_printf("v_stat returning %d mode=0%o file size=%d\n", r, buf->st_mode, (int)buf->st_size);
#endif
        return _set_lfs_error(r);
    }
    
    mode = S_IRUSR | S_IRGRP | S_IROTH;
    mode |= S_IWUSR | S_IWGRP | S_IWOTH;
    if (finfo.type == LFS_TYPE_DIR) {
#ifdef _DEBUG_LFS
        __builtin_printf("v_stat: is a directory\n");
#endif        
        mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
    }
    buf->st_mode = mode;
    buf->st_nlink = 1;
    buf->st_size = finfo.size;
    buf->st_blksize = 256;
    buf->st_blocks = (buf->st_size+255) / 256;
    buf->st_atime = buf->st_mtime = buf->st_ctime = 0;
#ifdef _DEBUG_LFS
    __builtin_printf("v_stat returning %d mode=0%o file size=%d\n", r, buf->st_mode, (int)buf->st_size);
#endif
    return r;    
}

static int v_opendir(DIR *dir, const char *name)
{
    lfs_dir_t *f = malloc(sizeof(*f));
    int r;

    if (name[0] == 0) {
        name = ".";
    }
#ifdef _DEBUG_LFS    
    __builtin_printf("v_opendir(%s)\n", name);
#endif    
    if (!f) {
#ifdef _DEBUG_LFS
      __builtin_printf("malloc failed\n");
#endif    
      return _seterror(ENOMEM);
    }
    r = lfs_dir_open(&lfs, f, name);
#ifdef _DEBUG_LFS
    __builtin_printf("lfs_dir_open returned %d\n", r);
#endif    
    
    if (r) {
        free(f);
        return _set_lfs_error(r);
    }
    dir->vfsdata = f;
    return 0;
}

static int v_closedir(DIR *dir)
{
    int r;
    lfs_dir_t *f = dir->vfsdata;
    r = lfs_dir_close(&lfs, f);
    free(f);
#ifdef _DEBUG_LFS
    __builtin_printf("lfs_dir_close returned %d\n", r);
#endif    
    return _set_lfs_error(r);
}

static int v_readdir(DIR *dir, struct dirent *ent)
{
    struct lfs_info finfo;
    int r;

    r = lfs_dir_read(&lfs, dir->vfsdata, &finfo);
#ifdef _DEBUG_LFS
    __builtin_printf("readdir fs_read: %d\n", r);
#endif	
    if (r < 0) {
        return _set_lfs_error(-r); // error
    }
    if (finfo.name[0] == 0) {
#ifdef _DEBUG_LFS
    __builtin_printf("readdir fs_read: EOF\n");
#endif	
        return -1; // EOF
    }
#ifdef _DEBUG_LFS
    __builtin_printf("readdir fs_read: name=%s\n", finfo.name);
#endif	
    strcpy(ent->d_name, finfo.name);

    if (finfo.type & LFS_TYPE_DIR) {
        ent->d_type = DT_DIR;
    } else {
        ent->d_type = DT_REG;
    }
    ent->d_size = finfo.size;
    ent->d_mtime = 0;
    return 0;
}

unsigned long long f_pinmask;

/* initialize (do first mount) */
/* for now this is a dummy function, but eventually some of the work
 * in _vfs_open could be done here
 */
static int v_init(const char *mountname)
{
    return 0;
}

/* deinitialize (unmount) */
static int v_deinit(const char *mountname)
{
    lfs_unmount(&lfs);
    _freepins(f_pinmask);
    lfs_in_use = 0;
    return 0;
}


struct vfs *
get_vfs(struct littlefs_flash_config *fcfg, int do_format)
{
    struct vfs *v;
    int r;
#ifdef _DEBUG_LFS
    __builtin_printf("get_vfs for littlefs\n");
#endif
    if (lfs_in_use) {
#ifdef _DEBUG_LFS
        __builtin_printf("littlefs is in use\n");
#endif
        _seterror(EBUSY);
        return 0;
    }

    v = calloc(1, sizeof(struct vfs));
    if (!v) {
        _seterror(ENOMEM);
        return 0;
    }
    f_pinmask = fcfg->pinmask;
#ifdef _DEBUG_LFS
        unsigned hi = f_pinmask >> 32;
        unsigned lo = f_pinmask;
        __builtin_printf("littlefs: flash pins %x: %x\n", hi, lo);
#endif        
    if (!_usepins(f_pinmask)) {
#ifdef _DEBUG_LFS
        __builtin_printf("littlefs: flash pins %x: %x are in use\n", hi, lo);
#endif        
        _seterror(EBUSY);
        return 0;
    }
    r = _flash_create(&lfs_cfg, fcfg);
    if (r) {
#ifdef _DEBUG_LFS
        __builtin_printf("littlefs: _flash_create returned error\n");
#endif        
        _seterror(-r);
        return 0;
    }
    r = lfs_mount(&lfs, &lfs_cfg);
    if (r && do_format) {
        // try formatting the file system
        lfs_format(&lfs, &lfs_cfg);
        r = lfs_mount(&lfs, &lfs_cfg);
    }
    if (r) {
        _set_lfs_error(r);
        return 0;
    }
    lfs_in_use = 1;
    
    v->close = &v_close;
    v->read = &v_read;
    v->write = &v_write;
    v->lseek = &v_lseek;
    v->ioctl = &v_ioctl;
    v->flush = &v_flush;
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

    v->getcf = 0;
    v->putcf = 0;

    v->vfs_data = &lfs;
    return v;
}

int
v_mkfs(struct littlefs_flash_config *fcfg)
{
    int r;
    f_pinmask = (1ULL << 61) | (1ULL << 60) | (1ULL << 59) | (1ULL << 58);
    if (!_usepins(f_pinmask)) {
        return _seterror(EBUSY);
    }
    _flash_create(&lfs_cfg, fcfg);
    r = lfs_format(&lfs, &lfs_cfg);
    _freepins(f_pinmask);
    return _set_lfs_error(r);
}
