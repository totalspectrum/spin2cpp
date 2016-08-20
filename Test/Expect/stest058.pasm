PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_func
	add	objptr, #8
	rdlong	result1, objptr
	add	objptr, #4
	rdbyte	_tmp002_, objptr
	sub	objptr, #12
	add	result1, _tmp002_
_func_ret
	ret

_tmp002_
	long	0
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
	fit	496
	long
objmem
	long	$00000000
	long	$00000000
	long	$00000000
	long	$00000000
