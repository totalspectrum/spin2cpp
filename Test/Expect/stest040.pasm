PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_foo
	mov	_var_01, #5
LR__0001
	add	OUTA, #1
	djnz	_var_01, #LR__0001
_foo_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_01
	res	1
	fit	496
