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
	rdlong	bump_tmp001_, objptr
	sub	objptr, #4
	add	arg02, bump_tmp001_
	mov	arg01, arg02
	call	#_update
	rdlong	arg02, objptr
	add	objptr, #4
	rdlong	bump_tmp001_, objptr
	sub	objptr, #4
	add	arg02, bump_tmp001_
	mov	arg01, arg02
	call	#_update
_bump_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[2]
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
bump_tmp001_
	res	1
	fit	496
