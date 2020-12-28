pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get01
	add	ptr__dat__, #1
	rdbyte	result1, ptr__dat__
	sub	ptr__dat__, #1
	add	objptr, #1
	rdbyte	_var01, objptr
	sub	objptr, #1
	add	result1, _var01
	and	result1, #255
_get01_ret
	ret

_get10
	add	ptr__dat__, #7
	rdbyte	result1, ptr__dat__
	sub	ptr__dat__, #7
	add	objptr, #7
	rdbyte	_var01, objptr
	sub	objptr, #7
	add	result1, _var01
	and	result1, #255
_get10_ret
	ret

_get23
	add	ptr__dat__, #17
	rdbyte	result1, ptr__dat__
	sub	ptr__dat__, #17
	add	objptr, #17
	rdbyte	_var01, objptr
	sub	objptr, #17
	add	result1, _var01
	and	result1, #255
_get23_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00[36]
objmem
	long	0[9]
	org	COG_BSS_START
_var01
	res	1
	fit	496
