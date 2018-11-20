pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_clr
	mov	_var_00, #10
	add	objptr, #40
	mov	_var_01, objptr
	sub	objptr, #40
LR__0001
	sub	_var_01, #4
	mov	_tmp001_, #0
	wrlong	_tmp001_, _var_01
	djnz	_var_00, #LR__0001
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
_var_00
	res	1
_var_01
	res	1
	fit	496
