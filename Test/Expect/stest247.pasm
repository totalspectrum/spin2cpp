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

__system___float_mul
	mov	__system___float_mul_af, arg01
	mov	__system___float_mul_bf, arg02
	mov	__system___float_mul_aflag_0014, __system___float_mul_af
	mov	__system___float_mul__cse__0000, __system___float_mul_aflag_0014
	shl	__system___float_mul__cse__0000, #9
	mov	__system___float_mul__cse__0001, __system___float_mul__cse__0000
	shr	__system___float_mul__cse__0001, #9
	mov	__system___float_mul_a_0008, __system___float_mul__cse__0001
	mov	__system___float_mul__cse__0002, __system___float_mul_aflag_0014
	shl	__system___float_mul__cse__0002, #1
	mov	__system___float_mul__cse__0003, __system___float_mul__cse__0002
	shr	__system___float_mul__cse__0003, #24
	mov	__system___float_mul_aexp_0010, __system___float_mul__cse__0003
	shr	__system___float_mul_aflag_0014, #31
	mov	arg01, __system___float_mul_bf
	mov	result1, arg01
	mov	_system___float_mul_tmp001_, result1
	mov	__system___float_mul_bflag_0015, _system___float_mul_tmp001_
	mov	__system___float_mul__cse__0005, __system___float_mul_bflag_0015
	shl	__system___float_mul__cse__0005, #9
	mov	__system___float_mul__cse__0006, __system___float_mul__cse__0005
	shr	__system___float_mul__cse__0006, #9
	mov	__system___float_mul_b_0009, __system___float_mul__cse__0006
	mov	__system___float_mul__cse__0007, __system___float_mul_bflag_0015
	shl	__system___float_mul__cse__0007, #1
	mov	__system___float_mul__cse__0008, __system___float_mul__cse__0007
	shr	__system___float_mul__cse__0008, #24
	mov	__system___float_mul_bexp_0011, __system___float_mul__cse__0008
	shr	__system___float_mul_bflag_0015, #31
	mov	__system___float_mul_alo_0013, #0
	xor	__system___float_mul_aflag_0014, __system___float_mul_bflag_0015
	cmp	__system___float_mul_aexp_0010, #255 wz
 if_e	jmp	#LR__0003
	cmp	__system___float_mul_bexp_0011, #255 wz
 if_e	jmp	#LR__0005
	cmp	__system___float_mul_aexp_0010, #0 wz
 if_e	jmp	#LR__0006
	or	__system___float_mul_a_0008, imm_8388608_
LR__0001
	cmp	__system___float_mul_bexp_0011, #0 wz
 if_e	jmp	#LR__0009
	or	__system___float_mul_b_0009, imm_8388608_
LR__0002
	mov	arg03, __system___float_mul_aexp_0010
	add	arg03, __system___float_mul_bexp_0011
	sub	arg03, #254
	mov	muldiva_, __system___float_mul_a_0008
	shl	muldiva_, #4
	mov	muldivb_, __system___float_mul_b_0009
	shl	muldivb_, #5
	call	#unsmultiply_
	mov	arg02, muldiva_
	mov	arg01, muldivb_
	cmp	arg01, imm_16777216_ wc
 if_ae	add	arg03, #1
 if_ae	shr	arg02, #1
 if_ae	mov	arg04, arg01
 if_ae	shl	arg04, #31
 if_ae	or	arg02, arg04
 if_ae	shr	arg01, #1
	mov	arg04, __system___float_mul_aflag_0014
	call	#__system__pack_0007
	jmp	#__system___float_mul_ret
LR__0003
	cmp	__system___float_mul_a_0008, #0 wz
 if_ne	mov	result1, __system___float_mul_af
 if_ne	jmp	#__system___float_mul_ret
	cmps	__system___float_mul_bexp_0011, #255 wc
 if_b	cmp	__system___float_mul_bexp_0011, #0 wz
 if_c_and_nz	jmp	#LR__0004
 if_b	cmp	__system___float_mul_b_0009, #0 wz
 if_c_and_nz	jmp	#LR__0004
 if_b	mov	result1, imm_2146435072_
 if_b	jmp	#__system___float_mul_ret
	cmp	__system___float_mul_b_0009, #0 wz
 if_ne	mov	result1, __system___float_mul_bf
 if_ne	jmp	#__system___float_mul_ret
LR__0004
	mov	arg04, __system___float_mul_aflag_0014
	or	arg04, #2
	mov	arg03, __system___float_mul_aexp_0010
	mov	arg01, #0
	mov	arg02, #0
	call	#__system__pack_0007
	jmp	#__system___float_mul_ret
LR__0005
	cmp	__system___float_mul_aexp_0010, #0 wz
 if_e	cmp	__system___float_mul_a_0008, #0 wz
 if_e	mov	result1, imm_2146435072_
 if_e	jmp	#__system___float_mul_ret
	cmp	__system___float_mul_b_0009, #0 wz
 if_ne	mov	result1, __system___float_mul_bf
 if_ne	jmp	#__system___float_mul_ret
	mov	arg04, __system___float_mul_aflag_0014
	or	arg04, #2
	mov	arg03, __system___float_mul_bexp_0011
	mov	arg01, #0
	mov	arg02, #0
	call	#__system__pack_0007
	jmp	#__system___float_mul_ret
LR__0006
	cmp	__system___float_mul_a_0008, #0 wz
 if_e	jmp	#LR__0008
LR__0007
	shl	__system___float_mul_a_0008, #1
	cmp	__system___float_mul_a_0008, imm_8388608_ wc
 if_b	mov	_system___float_mul_tmp002_, __system___float_mul_aexp_0010
 if_b	sub	_system___float_mul_tmp002_, #1
 if_b	mov	__system___float_mul_aexp_0010, _system___float_mul_tmp002_
 if_b	jmp	#LR__0007
	jmp	#LR__0001
LR__0008
	mov	arg04, __system___float_mul_aflag_0014
	or	arg04, #8
	mov	arg01, #0
	mov	arg02, #0
	mov	arg03, #0
	call	#__system__pack_0007
	jmp	#__system___float_mul_ret
LR__0009
	cmp	__system___float_mul_b_0009, #0 wz
 if_e	jmp	#LR__0011
LR__0010
	shl	__system___float_mul_b_0009, #1
	cmp	__system___float_mul_b_0009, imm_8388608_ wc
 if_b	sub	__system___float_mul_bexp_0011, #1
 if_b	jmp	#LR__0010
	jmp	#LR__0002
LR__0011
	or	__system___float_mul_aflag_0014, #8
	mov	arg01, #0
	mov	arg02, #0
	mov	arg03, #0
	mov	arg04, __system___float_mul_aflag_0014
	call	#__system__pack_0007
__system___float_mul_ret
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
 if_ne	jmp	#LR__0013
	mov	_var03, arg01
	mov	_var04, #32
LR__0012
	shl	_var03, #1 wc
 if_ae	djnz	_var04, #LR__0012
	sub	_var04, #23
	mov	_var02, _var04
	mov	_var03, #7
	sub	_var03, _var04
	shl	arg01, _var03
LR__0013
	sub	_var02, #127
	mov	result3, arg01
	mov	result1, _var01
	mov	result2, _var02
	mov	_var05, result3
	cmps	result2, imm_4294967295_ wc
 if_b	jmp	#LR__0014
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
LR__0014
	mov	result1, #0
__system___float_tointeger_ret
	ret

__system__pack_0007
	mov	_var01, arg01
	mov	_var02, arg02
	mov	_var03, arg03
	mov	_var04, arg04
	add	_var03, #127
	test	_var04, #4 wz
 if_e	jmp	#LR__0015
	cmp	_var01, #0 wz
 if_e	mov	_var01, imm_4194304_
	or	_var01, imm_2139095040_
	jmp	#LR__0023
LR__0015
	test	_var04, #2 wz
 if_ne	mov	_var01, imm_2139095040_
 if_ne	mov	_var02, #0
 if_ne	jmp	#LR__0022
	test	_var04, #8 wz
 if_ne	mov	_var01, #0
 if_ne	jmp	#LR__0021
	cmps	_var03, #255 wc
 if_ae	mov	_var01, imm_2139095040_
 if_ae	mov	_var02, #0
 if_ae	jmp	#LR__0020
	cmps	_var03, #1 wc
 if_ae	jmp	#LR__0018
	shr	_var02, #1
	mov	_var05, _var01
	and	_var05, #1
	shl	_var05, #31
	or	_var02, _var05
LR__0016
	shr	_var01, #1
	cmps	_var03, #0 wc
 if_ae	jmp	#LR__0017
	cmp	_var01, #0 wz
 if_ne	mov	_var06, _var02
 if_ne	and	_var06, #1
 if_ne	add	_var03, #1
 if_ne	shr	_var02, #1
 if_ne	mov	_var07, _var01
 if_ne	and	_var07, #1
 if_ne	shl	_var07, #31
 if_ne	or	_var02, _var07
 if_ne	or	_var02, _var06
 if_ne	jmp	#LR__0016
LR__0017
	cmps	_var03, #0 wc
 if_ae	jmp	#LR__0019
	mov	_var08, #0
	cmp	_var02, #0 wz
 if_ne	mov	_var08, #1
	mov	_var02, _var08
	jmp	#LR__0019
LR__0018
	andn	_var01, imm_4286578688_
	shl	_var03, #23
	or	_var01, _var03
LR__0019
LR__0020
LR__0021
LR__0022
LR__0023
	test	_var01, #1 wz
 if_ne	or	_var02, #1
	mov	_var08, _var02
	mov	_var07, #0
	add	_var02, imm_2147483647_
	cmp	_var02, _var08 wc
 if_b	mov	_var07, #1
	add	_var01, _var07
	test	_var04, #1 wz
 if_ne	or	_var01, imm_2147483648_
	mov	result1, _var01
__system__pack_0007_ret
	ret

multiply_
	mov	itmp2_, muldiva_
	xor	itmp2_, muldivb_
	abs	muldiva_, muldiva_
	abs	muldivb_, muldivb_
	jmp	#do_multiply_

unsmultiply_
	mov	itmp2_, #0
do_multiply_
	mov	result1, #0
	mov	itmp1_, #32
	shr	muldiva_, #1 wc
mul_lp_
 if_c	add	result1, muldivb_ wc
	rcr	result1, #1 wc
	rcr	muldiva_, #1 wc
	djnz	itmp1_, #mul_lp_
	shr	itmp2_, #31 wz
	negnz	muldivb_, result1
 if_nz	neg	muldiva_, muldiva_ wz
 if_nz	sub	muldivb_, #1
multiply__ret
unsmultiply__ret
	ret

imm_1199570944_
	long	1199570944
imm_16777216_
	long	16777216
imm_2139095040_
	long	2139095040
imm_2146435072_
	long	2146435072
imm_2147483647_
	long	2147483647
imm_2147483648_
	long	-2147483648
imm_4194304_
	long	4194304
imm_4286578688_
	long	-8388608
imm_4294967295_
	long	-1
imm_536870912_
	long	536870912
imm_8388607_
	long	8388607
imm_8388608_
	long	8388608
itmp1_
	long	0
itmp2_
	long	0
muldiva_
	long	0
muldivb_
	long	0
result1
	long	0
result2
	long	1
result3
	long	2
COG_BSS_START
	fit	496
	org	COG_BSS_START
__system___float_mul__cse__0000
	res	1
__system___float_mul__cse__0001
	res	1
__system___float_mul__cse__0002
	res	1
__system___float_mul__cse__0003
	res	1
__system___float_mul__cse__0005
	res	1
__system___float_mul__cse__0006
	res	1
__system___float_mul__cse__0007
	res	1
__system___float_mul__cse__0008
	res	1
__system___float_mul_a_0008
	res	1
__system___float_mul_aexp_0010
	res	1
__system___float_mul_af
	res	1
__system___float_mul_aflag_0014
	res	1
__system___float_mul_alo_0013
	res	1
__system___float_mul_b_0009
	res	1
__system___float_mul_bexp_0011
	res	1
__system___float_mul_bf
	res	1
__system___float_mul_bflag_0015
	res	1
_system___float_mul_tmp001_
	res	1
_system___float_mul_tmp002_
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
_var08
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
arg04
	res	1
	fit	496
