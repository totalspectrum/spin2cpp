PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_set
	add	objptr, #9
	rdbyte	set_tmp001_, objptr
	sub	objptr, #5
	wrlong	set_tmp001_, objptr
	sub	objptr, #4
_set_ret
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
set_tmp001_
	long	0
	fit	496
	long
objmem
	long	$00000000
	long	$00000000
	long	$00000000
