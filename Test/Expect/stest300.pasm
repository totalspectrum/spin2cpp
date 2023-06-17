pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_program
	mov	arg01, #1
	wrlong	arg01, objptr
	mov	arg01, #2
	wrlong	arg01, objptr
	or	dira, #4
	or	outa, #4
	mov	arg01, #3
	wrlong	arg01, objptr
	or	dira, #8
	or	outa, #8
_program_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
arg01
	res	1
	fit	496
