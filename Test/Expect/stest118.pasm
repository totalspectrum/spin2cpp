pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

__float_fromuns
	cmp	arg01, #0 wz
 if_ne	mov	result1, arg01
 if_ne	and	result1, #15
 if_ne	shr	arg01, #2
 if_ne	or	result1, arg01
 if_e	mov	result1, #0
__float_fromuns_ret
	ret

__float_fromint
	abs	arg01, arg01 wc
 if_b	mov	__float_fromint_negate, #1
 if_ae	mov	__float_fromint_negate, #0
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
__float_fromint_negate
	res	1
arg01
	res	1
	fit	496
