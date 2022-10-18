'
' mark as used any pointers found in HUB between startaddr and endaddr
'
pri _gc_markhub(startaddr, endaddr) | ptr, flags, heap_base, heap_end
  (heap_base, heap_end) := _gc_ptrs
  repeat while (startaddr < endaddr)
    ptr := long[startaddr]
    startaddr += 4
    ptr := _gc_isvalidptr(heap_base, heap_end, ptr)
    if ptr __andthen__ not _gc_isFree(ptr)
      flags := word[ptr + OFF_FLAGS]
      flags &= !GC_OWNER_MASK
      flags |= GC_FLAG_INUSE | GC_OWNER_HUB
      word[ptr + OFF_FLAGS] := flags

pri _gc_markcog | cogaddr, ptr, heap_base, heap_end
  (heap_base, heap_end) := _gc_ptrs
  repeat cogaddr from 495 to 0
    ptr := __reg__[cogaddr]
    ptr := _gc_isvalidptr(heap_base, heap_end, ptr)
      if ptr
        word[ptr + OFF_FLAGS] |= GC_FLAG_INUSE
