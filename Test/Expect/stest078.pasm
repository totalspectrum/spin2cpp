PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_setout
	rdlong	setout_tmp002_, objptr
	or	DIRA, setout_tmp002_
	or	OUTA, setout_tmp002_
_setout_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
objptr
	long	@@@objmem
result1
	long	0
setout_tmp002_
	long	0
	fit	496
	long
objmem
	long	$00000000
