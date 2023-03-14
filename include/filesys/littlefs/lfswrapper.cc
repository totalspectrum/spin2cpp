#include <unistd.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <errno.h>

#include "lfs.c"

#define SPI_PROG_SIZE          256

static char read_cache[SPI_PROG_SIZE];
static char prog_cache[SPI_PROG_SIZE];
static char lookahead_cache[SPI_PROG_SIZE];

typedef struct __using("filesys/littlefs/SpiFlash.spin2") _SpiFlash;

typedef struct _buffered_lfs_file {
    struct _default_buffer b;
    lfs_file_t       fd;
} BufferedLfsFile;

static _SpiFlash default_spi;

static int _flash_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    _SpiFlash *spi = cfg->context;
    unsigned long flashAdr = block * cfg->block_size + off;
    spi->Read(buffer, flashAdr, size);
    return 0;
}

static int _flash_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer_orig, lfs_size_t size) {
    _SpiFlash *spi = cfg->context;
    char *buffer = buffer_orig;
    unsigned long flashAdr = block * cfg->block_size + off;
    unsigned PAGE_SIZE = cfg->prog_size;
    unsigned PAGE_MASK = PAGE_SIZE-1; // assumes PAGE_SIZE is a power of 2

#ifdef _DEBUG_LFS
    __builtin_printf(" *** flash_prog: block=%d flashAdr=%x size=%x\n", block, flashAdr, size);
#endif        
    // make sure size and address are page multiples
    if ( (flashAdr & PAGE_MASK) || (size & PAGE_MASK) ) {
        return -EINVAL;
    }
    while (size > 0) {
        spi->WritePage(buffer, flashAdr);
        buffer += PAGE_SIZE;
        flashAdr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    return 0;
}

static int _flash_erase(const struct lfs_config *cfg, lfs_block_t block) {
    _SpiFlash *spi = cfg->context;
    unsigned long flashAdr = block * cfg->block_size;

#ifdef _DEBUG_LFS
    __builtin_printf(" *** flash_erase: flashAdr=%x\n", flashAdr);
#endif        
    // check if erase is valid
    if (block >= cfg->block_count)
        return -1;

    spi->Erase(flashAdr);
    return 0;
}

static int _flash_sync(const struct lfs_config *cfg) {
    return 0;
}

static int _flash_create(struct lfs_config *cfg, struct littlefs_flash_config *flashcfg)
{
    _SpiFlash *spi = &default_spi;
    cfg->context = (void *)spi;
    
    if (flashcfg->page_size != SPI_PROG_SIZE) {
        return -EINVAL;
    }
    if (flashcfg->used_size % flashcfg->erase_size != 0) {
        return -EINVAL;
    }
    if (flashcfg->offset % flashcfg->erase_size != 0) {
        return -EINVAL;
    }
    spi->Init(flashcfg->offset, flashcfg->used_size, flashcfg->erase_size);
    
    // set up flash properties
    cfg->read_size = SPI_PROG_SIZE;
    cfg->prog_size = SPI_PROG_SIZE;
    cfg->block_size = flashcfg->erase_size;
    cfg->block_count = flashcfg->used_size / flashcfg->erase_size;
    cfg->cache_size = SPI_PROG_SIZE;
    cfg->lookahead_size = SPI_PROG_SIZE;
    cfg->block_cycles = 400;

    // buffers
    cfg->read_buffer = &read_cache;
    cfg->prog_buffer = &prog_cache;
    cfg->lookahead_buffer = &lookahead_cache;
    
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

static char lfs_in_use;
static lfs_t lfs;
static struct lfs_config lfs_cfg;

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
    __builtin_printf("v_lseek(%d, %d) ", offset, whence);
#endif
    if (!f) {
        return _seterror(EBADF);
    }
    r = lfs_file_seek(&lfs, &f->fd, offset, whence);
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

static unsigned long long f_pinmask;

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
    static struct vfs temp;
    int r;
#ifdef _DEBUG_LFS
    __builtin_printf("get_vfs for littlefs\n");
#endif
    if (lfs_in_use) {
        _seterror(EBUSY);
        return 0;
    }

    v = &temp;
    if (!v) {
        _seterror(ENOMEM);
        return 0;
    }
    f_pinmask = (1ULL << 61) | (1ULL << 60) | (1ULL << 59) | (1ULL << 58);
    if (!_usepins(f_pinmask)) {
        _seterror(EBUSY);
        return 0;
    }
    r = _flash_create(&lfs_cfg, fcfg);
    if (r) {
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
