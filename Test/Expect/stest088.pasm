PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_clr
	mov	_var_01, #10
	add	objptr, #40
	mov	_var_02, objptr
	sub	objptr, #40
LR__0001
	sub	_var_02, #4
	mov	_tmp001_, #0
	wrlong	_tmp001_, _var_02
	djnz	_var_01, #LR__0001
_clr_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_tmp001_
	res	1
_var_01
	res	1
_var_02
	res	1
	fit	496
