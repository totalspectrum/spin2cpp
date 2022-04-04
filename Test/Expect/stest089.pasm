pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	_var01, cnt
	mov	_var02, imm_512_
LR__0001
	xor	outa, #1
	djnz	_var02, #LR__0001
	mov	_var02, cnt
	sub	_var02, _var01
	mov	dira, _var02
_main_ret
	ret

imm_512_
	long	512
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
	fit	496
