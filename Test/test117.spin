{ comment test }
VAR
  long a

PUB getit | i
  '' start something
  a:=0

  '' count up
  repeat i from 1 to 5
    '' update a
    a += i
  '' now all done
  return 0

{{
  This comment is at end of file
}}
