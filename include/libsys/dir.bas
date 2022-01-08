'
' directory handling functions for BASIC
'

declare function _basic_dir lib "libsys/basic_dir.c" (pat as string, attrib as integer) as string
declare function getcwd lib "libc/unix/mount.c" (buf as ubyte pointer, size as integer) as ubyte pointer

function dir$(pat = "" as string, attrib = 0 as integer) as string
    return _basic_dir(pat, attrib)
end function

function curdir$() as string
  dim buf as ubyte pointer
  dim r as ubyte pointer
  var bufsize = 64
  buf = new ubyte(bufsize)
  r = getcwd(buf, bufsize)
  if r == 0 then
    bufsize *= 4
    buf = new ubyte(bufsize)
    r = getcwd(buf, bufsize)
  endif
  return r
end function
