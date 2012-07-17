dat
   byte 1

pub start
   cognew(@entry, 0)

dat
   org 0
entry
   mov t1, par

t1 long $11223344
