
{{
  Garbage collecting allocator

  Use:

  Initialize with _gc_init(heap_base, size)
  
  Allocate memory with "ptr = _gc_alloc(size)"
    This returns 0 if there is not enough free space left, or if
    size is 0. Otherwise it returns a pointer to memory.
    
    The memory is allocated in "reserved" state, which means that the
    garbage collector will never free it. This is because the garbage
    collector can only observe references from HUB memory, so if the
    initial reference is from COG memory we do not want to accidentally
    free it.

  You have two options for freeing the memory:
    (1) Explicitly free it via _gc_free(ptr). "ptr" must be the original,
    unmodified pointer returned from _gc_alloc.
    
    (2) Mark it as safe to garbage collect via _gc_manage(ptr). In this
    case the memory will be left up to the garbage collector. If you want
    to keep the memory in use, make sure there is a copy of "ptr" stored in
    a global variable, on the stack, or in the heap (memory allocated via
    _gc_alloc()). This copy *must* be on a longword boundary, and the
    pointer must not have been modified in any way. As long as there is such
    a reference copy, the garbage collector will leave the memory alone. If
    the garbage collector ever finds that "ptr" is no longer referenced from
    HUB memory or the COG memory of the cog that allocated it, then it may
    free the memory pointed to by "ptr" and re-allocate it for other purposes.

  There's also a _gc_alloc_managed call which is like a combination of
  _gc_alloc followed immediately by _gc_manage.
  
INTERNALS

    Memory is allocated in 16 byte pages, and we use a 16 bit size field
    (for size in pages) in the block. That means the maximum allocation
    size is 65536 * 16, or 1 MB. This is the HUB memory size of the P2, so
    it works out great. For larger allocations, use a bigger page size
    or change the header to use 32 bit indexes instead of 16 bit ones.

    Blocks have an 8 byte header, consisting of 4 WORDS:
    size = size of this block in pages (may be used to infer next block)
    flags = flags (see below)
    prev = pointer to prev block in system
    link = link field for free list

    flag meaning:
    low 4 bits: COG owner
      0-7 == cog that allocated this
      $e == known to be in HUB memory (so no specific COG owner)
      $f == free
    $10 = reserved   (never free automatically)
    $20 = inuse      (block was observed to be in use during GC)
    the upper 10 bits of flags should be GC_MAGIC, used for sanity checking
    
  ALSO OF NOTE
  block 0 is reserved, and serves as the anchor for the free list
}}

con
  __real_heapsize__ = 256 ' size of heap in longs, may be overridden by user
  pagesize = 16         ' size of page in bytes
  pagesizeshift = 4     ' log2(pagesize)
  headersize = 8        ' length of block header
  pagemask = pagesize - 1
  GC_MAGIC = $6c80
  GC_MAGIC_MASK = $ffc0
  GC_FLAG_FREE = $000f
  GC_OWNER_HUB = $000e
  GC_OWNER_MASK = $000f
  GC_FLAG_RESERVED = $0010
  GC_FLAG_INUSE = $0020
  
  ' byte offsets in header
  OFF_SIZE = 0
  OFF_FLAGS = 2
  OFF_PREV = 4
  OFF_LINK = 6

  ' special offsets for block 0
  OFF_USED_LINK = 8

  ' magic constant added to pointers so we can spot them
  ' more easily
  POINTER_MAGIC =      $63800000
  POINTER_MAGIC_MASK = $fff00000

dat
_memory_mutex
  long 0

  
''
'' return gc pointers
'' if called before gc pointers are set up, initialize
'' the heap
''
pri _gc_ptrs : base, end | size
  base := __get_heap_base  ' __heap_ptr is a special variable
  end := base + (__real_heapsize__*4) - headersize
  if (long[base] == 0)
    size := end - base
    word[base + OFF_SIZE] := 1 
    word[base + OFF_FLAGS] := GC_MAGIC | GC_FLAG_RESERVED
    word[base + OFF_PREV] := 0
    word[base + OFF_LINK] := 1
    base += pagesize
    word[base + OFF_SIZE] := (size / pagesize)
    word[base + OFF_FLAGS] := GC_MAGIC | GC_FLAG_FREE
    word[base + OFF_PREV] := 0
    word[base + OFF_LINK] := 0
    base -= pagesize
  return (base, end)

{ return a pointer to page i in the heap }
pri _gc_pageptr(heapbase, i)
  if (i == 0)
    return 0
  return heapbase + (i << pagesizeshift)

pri _gc_pageindex(heapbase, ptr)
  if (ptr == 0)
    return 0
  return (ptr - heapbase) >> pagesizeshift
  
pri _gc_isFree(ptr)
  return word[ptr + OFF_FLAGS] == GC_MAGIC + GC_FLAG_FREE

' go to the next block in the global list of blocks
pri _gc_nextBlockPtr(ptr) | t
  t := word[ptr + OFF_SIZE]
  if t == 0
    return _gc_errmsg(string(" !!! corrupted heap??? !!! "))
  return ptr + (t << pagesizeshift)
  
pri _gc_tryalloc(size, reserveflag) : ptr | availsize, lastptr, nextptr, heap_base, heap_end, saveptr, linkindex
  (heap_base, heap_end) := _gc_ptrs()
  ptr := heap_base
  availsize := 0
  
  repeat
    lastptr := ptr
    ptr := _gc_pageptr(heap_base, word[ptr+OFF_LINK])
    if ptr
      availsize := word[ptr+OFF_SIZE]
  while ptr and ptr < heap_end and size > availsize

  if (ptr == 0)
    return ptr

  linkindex := word[ptr + OFF_LINK]
  
  '' carve off free space if necessary
  if (size < availsize)
    '' shrink this block, link to newly created block
    word[ptr + OFF_SIZE] := size
    nextptr := ptr + (size<<pagesizeshift)
    word[nextptr + OFF_SIZE] := availsize - size
    word[nextptr + OFF_FLAGS] := GC_MAGIC | GC_FLAG_FREE
    word[nextptr + OFF_PREV] := _gc_pageindex(heap_base, ptr)
    word[nextptr + OFF_LINK] := word[ptr + OFF_LINK]
    '' advance to next in chain
    saveptr := nextptr
    linkindex := _gc_pageindex(heap_base, saveptr)
    nextptr := _gc_nextBlockPtr(nextptr)
    if (nextptr and nextptr <  heap_end)
      word[nextptr + OFF_PREV] := _gc_pageindex(heap_base, saveptr)
     
  '' now unlink us from the free list
  word[lastptr + OFF_LINK] := linkindex
  
  '' mark as used, reserved, owned by a cog
  word[ptr + OFF_FLAGS] := GC_MAGIC | reserveflag | _cogid()
  
  '' link us to the used list
  word[ptr + OFF_LINK] := word[heap_base + OFF_USED_LINK]
  word[heap_base + OFF_USED_LINK] := _gc_pageindex(heap_base, ptr)
  
  '' and return
  return (ptr + headersize) | POINTER_MAGIC

pri _gc_errmsg(s) | c
  repeat while ((c:=byte[s++]) <> 0)
    _tx(c)
  return 0

pri _gc_errhex(h)
  _txhex(h)
  
pri _gc_alloc(size)
  return _gc_doalloc(size, GC_FLAG_RESERVED)

pri _gc_alloc_managed(size) : r
  r := _gc_doalloc(size, 0)
  if (r == 0 and size > 0)
    return _gc_errmsg(string(" !!! out of heap memory !!! "))
  return r

pri _gc_doalloc(size, reserveflag) : ptr | zptr
  if (size == 0)
    return ptr  ' defaults to 0

  ' increase size request to include the header
  size += headersize
  size := (size + pagemask) & !pagemask
  ' convert to pages
  size := size >> pagesizeshift

  _lockmem(@_memory_mutex)

  ' try to find a free block big enough
  ptr := _gc_tryalloc(size, reserveflag)

  if (ptr == 0)
    '' run garbage collection here
    _gc_docollect
    ' see if gc freed up enough space
    ptr := _gc_tryalloc(size, reserveflag)
    
  _unlockmem(@_memory_mutex)
    
  if ptr
    ' zero the returned memory
    size := ((size << pagesizeshift) - headersize)/4
    zptr := ptr ' skip the header
    repeat size
      long[zptr] := 0
      zptr += 4
  return ptr

'
' returns 0 if ptr is not a valid pointer
' otherwise returns the start of the block it
' points to
'
pri _gc_isvalidptr(base, heap_end, ptr) | t
  ' make sure the poiter came from alloc
  if (ptr & POINTER_MAGIC_MASK) <> POINTER_MAGIC
    return 0
  ' step back to the header
  ptr := (ptr - headersize) & (!POINTER_MAGIC_MASK)
  ' make sure it is in the heap
  if (ptr < base) or (ptr => heap_end)
    return 0

  ' and appropriately aligned (same as _gc_heap_base)
  if ( (ptr ^ base) & pagemask) <> 0
    return 0

  ' and make sure the GC_MAGIC bits are set in flags
  t := word[ptr + OFF_FLAGS]
  if (t & GC_MAGIC_MASK) <> GC_MAGIC
    return 0
  return ptr

'
' free a pointer previously returned by alloc
'
pri _gc_free(ptr) | heapbase, heapend
  (heapbase, heapend) := _gc_ptrs
  ptr := _gc_isvalidptr(heapbase, heapend, ptr)
  if (ptr)
    _lockmem(@_memory_mutex)
    _gc_dofree(ptr)
    _unlockmem(@_memory_mutex)

'
' un-reserve a pointer previously returned by alloc
'
pri _gc_manage(ptr) | heapbase, heapend
  (heapbase, heapend) := _gc_ptrs
  ptr := _gc_isvalidptr(heapbase, heapend, ptr)
  if (ptr)
    word[ptr + OFF_FLAGS] &= !GC_FLAG_RESERVED
    
'
' internal routine for freeing a block of memory
' ptr points to the start of the block
' returns a pointer to the next non-free block(useful for
' garbage collection, to handle cases where memory is coalesced)
'
pri _gc_dofree(ptr) | prevptr, tmpptr, nextptr, heapbase, heapend, n
  (heapbase, heapend) := _gc_ptrs
  word[ptr + OFF_FLAGS] := GC_MAGIC + GC_FLAG_FREE
  prevptr := ptr
  nextptr := _gc_nextBlockPtr(ptr)
  repeat
    '' walk back the chain to the last previous free block
    prevptr := _gc_pageptr(heapbase, word[prevptr + OFF_PREV])
  until prevptr == 0 __orelse__ _gc_isFree(prevptr)
  if (prevptr == 0)
    prevptr := heapbase
    
  word[ptr + OFF_LINK] := word[prevptr + OFF_LINK]
  word[prevptr + OFF_LINK] := _gc_pageindex(heapbase, ptr)

  ' see if we should merge with the previous block
  if (prevptr <> heapbase)
    tmpptr := _gc_nextBlockPtr(prevptr)
    if (tmpptr == ptr)
      word[prevptr + OFF_SIZE] += word[ptr + OFF_SIZE]
      word[ptr + OFF_FLAGS] := 0
      ' adjust prev link in next block
      nextptr := _gc_nextBlockPtr(ptr)
      if (nextptr < heapend)
        word[nextptr + OFF_PREV] := _gc_pageindex(heapbase, prevptr)
      word[prevptr + OFF_LINK] := word[ptr + OFF_LINK]
      word[ptr + OFF_LINK] := 0
      ptr := prevptr
      
  '' see if we should merge with following block
  tmpptr := _gc_nextBlockPtr(ptr)
  if tmpptr __andthen__ (tmpptr < heapend) __andthen__ _gc_isFree(tmpptr)
    prevptr := ptr
    ptr := tmpptr
    word[prevptr + OFF_SIZE] += word[ptr + OFF_SIZE]
    word[prevptr + OFF_LINK] := word[ptr + OFF_LINK]
    word[ptr + OFF_FLAGS] := $AA
    word[ptr + OFF_LINK] := 0
    nextptr := _gc_nextBlockPtr(ptr)
    if (nextptr and nextptr < heapend)
      word[nextptr + OFF_PREV] := _gc_pageindex(heapbase, prevptr)

  return nextptr
   
''
'' user accessible garbage collection
''
pri _gc_collect
  _lockmem(@_memory_mutex)
  _gc_docollect
  _unlockmem(@_memory_mutex)
  
''
'' actual garbage collection routine
''
pri _gc_docollect | ptr, nextptr, startheap, endheap, flags, ourid, size

  (startheap, endheap) := _gc_ptrs

  ' clear the "IN USE" flags for all blocks
  ptr := _gc_nextBlockPtr(startheap)
  ourid := _cogid()
  repeat while ptr and ptr < endheap
    word[ptr + OFF_FLAGS] &= !GC_FLAG_INUSE
    ptr := _gc_nextBlockPtr(ptr)

  ' now mark pointers found in HUB memory
  ' this encompasses more than we need;
  ' find a way to get the end of code and
  ' start of stack??
  _gc_markhub(0, __topofstack(0))

  ' FIXME: ideally the above would get just the data section and stack
  ' and we would _gc_markhub() here for the heap

  'now mark everything found in COG memory
  _gc_markcog

  ' now free all pointers that aren't in use
  ' or reserved (or under another COG's control)

  ' there will always be at least one block after startheap,
  ' so nextptr will be valid at first
  nextptr := _gc_nextBlockPtr(startheap)
  if nextptr == 0
    _gc_errmsg(string(" !!! corrupted heap !!! "))
    return
    
  repeat
    ptr := nextptr
    nextptr := _gc_nextBlockPtr(ptr)
    flags := word[ptr + OFF_FLAGS]
    if ( (not (flags & GC_FLAG_INUSE)) and (not (flags & GC_FLAG_RESERVED)) )
         flags &= GC_OWNER_MASK
         if (flags == ourid) or (flags == GC_OWNER_HUB)
           nextptr := _gc_dofree(ptr)  ' dofree returns address of next block
           
  while (nextptr <> 0) and (nextptr < endheap)
  
