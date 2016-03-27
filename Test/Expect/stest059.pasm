PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_func
	add	objptr, #8
	rdlong	func_tmp001_, objptr
	add	objptr, #4
	rdbyte	func_tmp002_, objptr
	add	func_tmp001_, func_tmp002_
	sub	objptr, #8
	wrlong	func_tmp001_, objptr
	sub	objptr, #4
_func_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
func_tmp001_
	long	0
func_tmp002_
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
