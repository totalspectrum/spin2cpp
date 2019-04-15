''
'' This is a header file to set things up to compile classic BASIC games
'' from the classicbasicgames.org website.
'' Besides needing this header, you may also want to insert some spaces
'' into various PRINT statements (fastspin never inserts spaces, whereas
'' the classic basic games seem to expect them inserted around ;)
''
option implicit
defsng a-z
function tab(n as integer) as string
  return chr$(7)
end function

