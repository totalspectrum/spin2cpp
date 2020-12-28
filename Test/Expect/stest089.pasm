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
	mov	_var03, cnt
	sub	_var03, _var01
	mov	dira, _var03
_main_ret
	ret

__lockreg
	long	0
imm_512_
	long	512
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
	fit	496
