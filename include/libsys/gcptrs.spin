'
' helper function to get heap pointers
' this is broken out into its own file to make sure it gets resolved
' later (after user HEAPSIZE is defined)
' __real_heapsize__ gets set by the compiler based on the user HEAPSIZE
'
dat
        alignl
_gc_heap_base
	long 0[__real_heapsize__]
	'' buffer for gc
	long 0[2]
	
''
'' return gc pointers
'' if called before gc pointers are set up, initialize
'' the heap
''
pri _gc_ptrs : base, end | size
  base := @_gc_heap_base
  end := base + __real_heapsize__
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
