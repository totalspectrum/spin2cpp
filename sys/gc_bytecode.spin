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

' in bytecode there is no COG memory used
pri _gc_markcog
  return
