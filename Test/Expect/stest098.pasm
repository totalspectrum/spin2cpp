pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_Unpack_x
	mov	arg02, #0
	call	#_dounpack_x
_Unpack_x_ret
	ret

_Unpack_m
	mov	arg02, #1
	call	#_dounpack_x
_Unpack_m_ret
	ret

_dounpack_x
	mov	_var01, arg01
	shl	_var01, #1
	shr	_var01, #24 wz
	and	arg01, imm_8388607_
 if_ne	shl	arg01, #6
 if_ne	or	arg01, imm_536870912_
 if_ne	jmp	#LR__0002
	mov	_var02, arg01
	mov	_var03, #32
LR__0001
	shl	_var02, #1 wc
 if_ae	djnz	_var03, #LR__0001
	sub	_var03, #23
	mov	_var01, _var03
	mov	_var02, #7
	sub	_var02, _var03
	shl	arg01, _var02
LR__0002
	sub	_var01, #127
	cmp	arg02, #0 wz
 if_ne	mov	result1, _var01
 if_e	mov	result1, arg01
_dounpack_x_ret
	ret

imm_536870912_
	long	536870912
imm_8388607_
	long	8388607
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
