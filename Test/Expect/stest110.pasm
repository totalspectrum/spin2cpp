pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_filllong
loop
	wrlong	arg2, arg1
	add	arg1, #4
	djnz	arg3, #loop
_filllong_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
	fit	496
