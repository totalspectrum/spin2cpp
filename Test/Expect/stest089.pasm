PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_main
	mov	_var_00, CNT
	mov	_var_01, imm_512_
LR__0001
	xor	OUTA, #1
	djnz	_var_01, #LR__0001
	mov	_tmp001_, CNT
	sub	_tmp001_, _var_00
	mov	DIRA, _tmp001_
_main_ret
	ret

imm_512_
	long	512
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
_var_00
	res	1
_var_01
	res	1
	fit	496
