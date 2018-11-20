pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	_var_00, #5
LR__0001
	add	outa, #1
	djnz	_var_00, #LR__0001
_foo_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_00
	res	1
	fit	496
