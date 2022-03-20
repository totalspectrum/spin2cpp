pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

__float_fromuns
	cmp	arg01, #0 wz
 if_e	jmp	#LR__0001
	mov	_var01, arg01
	and	_var01, #15
	shr	arg01, #2
	mov	result1, #0
	or	result1, _var01
	or	result1, arg01
	jmp	#__float_fromuns_ret
LR__0001
	mov	result1, #0
__float_fromuns_ret
	ret

__float_fromint
	mov	__float_fromint_integer, arg01
	cmps	__float_fromint_integer, #0 wc
 if_b	neg	__float_fromint_integer, __float_fromint_integer
 if_b	mov	__float_fromint_negate, #1
 if_ae	mov	__float_fromint_negate, #0
	mov	arg01, __float_fromint_integer
	call	#__float_fromuns
	cmp	__float_fromint_negate, #0 wz
 if_ne	xor	result1, imm_2147483648_
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
_var01
	res	1
arg01
	res	1
	fit	496
