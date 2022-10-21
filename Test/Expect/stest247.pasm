pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_toReal
	mov	arg02, imm_1199570944_
	call	#__system___float_mul
	mov	arg01, result1
	mov	arg02, #0
	call	#__system___float_tointeger
_toReal_ret
	ret

__system___float_tointeger
	mov	_var01, arg01
	shr	_var01, #31
	mov	_var02, arg01
	shl	_var02, #1
	shr	_var02, #24 wz
	and	arg01, imm_8388607_
 if_ne	shl	arg01, #6
 if_ne	or	arg01, imm_536870912_
 if_ne	jmp	#LR__0002
	mov	_var03, arg01
	mov	_var04, #32
LR__0001
	shl	_var03, #1 wc
 if_ae	djnz	_var04, #LR__0001
	sub	_var04, #23
	mov	_var02, _var04
	mov	_var03, #7
	sub	_var03, _var04
	shl	arg01, _var03
LR__0002
	sub	_var02, #127
	mov	result3, arg01
	mov	result1, _var01
	mov	result2, _var02
	mov	_var05, result3
	cmps	result2, imm_4294967295_ wc
 if_b	jmp	#LR__0003
	cmps	result2, #31 wc
 if_b	shl	_var05, #2
 if_b	mov	_var06, #30
 if_b	sub	_var06, result2
 if_b	shr	_var05, _var06
 if_b	add	_var05, arg02
 if_b	shr	_var05, #1
 if_b	cmp	result1, #0 wz
 if_c_and_nz	neg	_var05, _var05
 if_b	mov	result1, _var05
 if_b	jmp	#__system___float_tointeger_ret
LR__0003
	mov	result1, #0
__system___float_tointeger_ret
	ret

imm_1199570944_
	long	1199570944
imm_4294967295_
	long	-1
imm_536870912_
	long	536870912
imm_8388607_
	long	8388607
result1
	long	0
result2
	long	1
result3
	long	2
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
_var05
	res	1
_var06
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
