pub blinky
  coginit(8, @entry, 15<<4)

dat
   org 0
entry
	mov pin, par
	shr pin, #4
	mov mask, #1
	shl mask, pin
	mov dira, mask
	mov outa, mask
	rdlong pause, #0
	mov    nextcnt, pause
	add	  nextcnt, cnt
loop
	waitcnt nextcnt, pause
	xor     outa, mask
	jmp	   #loop

pin
	long 0
mask
	long 0
pause
	long 0
nextcnt
	long 0

