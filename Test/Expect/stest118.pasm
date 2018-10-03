PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

__float_fromuns
	mov	_var_04, arg1 wz
 if_e	jmp	#LR__0001
	mov	_var_02, #0
	mov	_var_05, _var_04
	and	_var_05, #15
	mov	_var_03, _var_05
	shr	_var_04, #2
	mov	_var_07, #0
	or	_var_07, _var_03
	mov	_tmp001_, _var_07
	or	_tmp001_, _var_04
	mov	_var_08, _tmp001_
	mov	result1, _var_08
	jmp	#__float_fromuns_ret
LR__0001
	mov	result1, #0
__float_fromuns_ret
	ret

__float_fromint
	mov	__float_fromint_integer, arg1
	cmps	__float_fromint_integer, #0 wc,wz
 if_b	neg	__float_fromint_integer, __float_fromint_integer
 if_b	mov	__float_fromint_negate, #1
 if_ae	mov	__float_fromint_negate, #0
	mov	arg1, __float_fromint_integer
	call	#__float_fromuns
	mov	__float_fromint_single, result1
	cmp	__float_fromint_negate, #0 wz
 if_e	jmp	#LR__0002
	mov	__float_fromint_integer, __float_fromint_single
	mov	arg1, __float_fromint_integer
	xor	arg1, imm_2147483648_
	mov	result1, arg1
	mov	__float_fromint_single, result1
LR__0002
	mov	result1, __float_fromint_single
__float_fromint_ret
	ret

imm_2147483648_
	long	-2147483648
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
__float_fromint_integer
	res	1
__float_fromint_negate
	res	1
__float_fromint_single
	res	1
_tmp001_
	res	1
_var_02
	res	1
_var_03
	res	1
_var_04
	res	1
_var_05
	res	1
_var_07
	res	1
_var_08
	res	1
arg1
	res	1
	fit	496
