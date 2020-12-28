pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_update
	wrlong	arg01, objptr
	add	objptr, #4
	wrlong	arg02, objptr
	sub	objptr, #4
_update_ret
	ret

_bump
	rdlong	arg02, objptr
	add	objptr, #4
	rdlong	bump_tmp002_, objptr
	sub	objptr, #4
	add	arg02, bump_tmp002_
	wrlong	arg02, objptr
	add	objptr, #4
	wrlong	arg02, objptr
	sub	objptr, #4
	rdlong	arg02, objptr
	add	objptr, #4
	rdlong	bump_tmp002_, objptr
	sub	objptr, #4
	add	arg02, bump_tmp002_
	wrlong	arg02, objptr
	add	objptr, #4
	wrlong	arg02, objptr
	sub	objptr, #4
_bump_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
objmem
	long	0[2]
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
bump_tmp002_
	res	1
	fit	496
