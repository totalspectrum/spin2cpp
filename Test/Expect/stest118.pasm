pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

__float_fromuns
	mov	_var01, arg01 wz
 if_e	jmp	#LR__0001
	mov	_var02, #0
	mov	_var03, _var01
	and	_var03, #15
	mov	_var04, _var03
	shr	_var01, #2
	mov	_var05, #0
	or	_var05, _var04
	mov	_var06, _var05
	or	_var06, _var01
	mov	_var07, _var06
	mov	result1, _var07
	jmp	#__float_fromuns_ret
LR__0001
	mov	result1, #0
__float_fromuns_ret
	ret

__float_fromint
	mov	__float_fromint_integer, arg01
	cmps	__float_fromint_integer, #0 wc,wz
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
_var07
	res	1
arg01
	res	1
	fit	496
