pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	_var01, #80 wz
LR__0001
	mov	outa, _var01
	djnz	_var01, #LR__0001
	mov	result1, #0
_foo_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
	fit	496
