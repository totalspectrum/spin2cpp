PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_Unpack_x
	mov	arg2, #0
	call	#_dounpack_x
_Unpack_x_ret
	ret

_Unpack_m
	mov	arg2, #1
	call	#_dounpack_x
_Unpack_m_ret
	ret

_dounpack_x
	mov	_var_07, arg1
	shl	_var_07, #1
	shr	_var_07, #24
	mov	_var_03, _var_07 wz
	and	arg1, imm_8388607_
 if_ne	shl	arg1, #6
 if_ne	or	arg1, imm_536870912_
 if_ne	jmp	#LR__0002
	mov	_tmp003_, arg1
	mov	_tmp002_, #32
LR__0001
	shl	_tmp003_, #1 wc
 if_nc	djnz	_tmp002_, #LR__0001
	sub	_tmp002_, #23
	mov	_var_05, _tmp002_
	mov	_var_03, _var_05
	mov	_var_11, #7
	sub	_var_11, _var_05
	shl	arg1, _var_11
LR__0002
	sub	_var_03, #127
	cmps	arg2, #0 wz
 if_ne	mov	result1, _var_03
 if_e	mov	result1, arg1
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
_tmp002_
	res	1
_tmp003_
	res	1
_var_03
	res	1
_var_05
	res	1
_var_07
	res	1
_var_11
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
