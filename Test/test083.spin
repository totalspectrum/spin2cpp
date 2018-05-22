dat

Header  byte "--", 13
	byte 0
Error   byte " !", 0

pub start
   cognew(@entry, 0)

dat
   org 0
entry
   mov t1, par

t1 long $11223344
