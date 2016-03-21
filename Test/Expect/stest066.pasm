DAT
	org	0

_set
	add	objptr, #8
	mov	set_tmp001_, objptr
	sub	objptr, #4
	rdlong	set_tmp002_, objptr
	sub	objptr, #4
	add	set_tmp001_, set_tmp002_
	wrbyte	arg1, set_tmp001_
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
set_tmp002_
	long	0
	fit	496
	long
objmem
	long	$00000000
	long	$00000000
	long	$00000000
