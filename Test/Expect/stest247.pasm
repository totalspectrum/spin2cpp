pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_toReal
	mov	arg02, imm_1199570944_
	call	#__system___float_Unpack
	mov	__system___float_mul_sa, result1
	mov	__system___float_mul_xa, result2
	mov	__system___float_mul_ma, result3
	mov	arg01, imm_1199570944_
	call	#__system___float_Unpack
	mov	__system___float_mul_sb, result1
	mov	__system___float_mul_xb, result2
	mov	muldivb_, result3
	xor	__system___float_mul_sa, __system___float_mul_sb
	add	__system___float_mul_xa, __system___float_mul_xb
	mov	muldiva_, __system___float_mul_ma
	call	#multiply_
	mov	arg03, muldivb_
	shl	arg03, #3
	mov	arg01, __system___float_mul_sa
	mov	arg02, __system___float_mul_xa
	call	#__system___float_Pack
	mov	arg01, result1
	mov	arg02, #0
	call	#__system___float_tointeger
_toReal_ret
	ret

__system___float_tointeger
	mov	__system___float_tointeger_r, arg02
	call	#__system___float_Unpack
	mov	__system___float_tointeger_x, result2
	mov	__system___float_tointeger_m, result3
	cmps	__system___float_tointeger_x, imm_4294967295_ wc,wz
 if_b	jmp	#LR__0001
	cmps	__system___float_tointeger_x, #30 wc,wz
 if_a	jmp	#LR__0001
	shl	__system___float_tointeger_m, #2
	mov	__system___float_tointeger__cse__0001, #30
	sub	__system___float_tointeger__cse__0001, __system___float_tointeger_x
	shr	__system___float_tointeger_m, __system___float_tointeger__cse__0001
	add	__system___float_tointeger_m, __system___float_tointeger_r
	shr	__system___float_tointeger_m, #1
	cmp	result1, #0 wz
 if_ne	neg	__system___float_tointeger_m, __system___float_tointeger_m
	mov	result1, __system___float_tointeger_m
	jmp	#__system___float_tointeger_ret
LR__0001
	mov	result1, #0
__system___float_tointeger_ret
	ret

__system___float_Unpack
	mov	_var01, arg01
	shr	_var01, #31
	mov	_var02, arg01
	shl	_var02, #1
	shr	_var02, #24 wz
	and	arg01, imm_8388607_
 if_ne	shl	arg01, #6
 if_ne	or	arg01, imm_536870912_
 if_ne	jmp	#LR__0003
	mov	_var03, arg01
	mov	_var04, #32
LR__0002
	shl	_var03, #1 wc
 if_nc	djnz	_var04, #LR__0002
	sub	_var04, #23
	mov	_var02, _var04
	mov	_var05, #7
	sub	_var05, _var04
	shl	arg01, _var05
LR__0003
	sub	_var02, #127
	mov	result3, arg01
	mov	result1, _var01
	mov	result2, _var02
__system___float_Unpack_ret
	ret

__system___float_Pack
	mov	_var01, #0
	cmp	arg03, #0 wz
 if_e	jmp	#LR__0006
	mov	_var02, arg03
	mov	_var03, #32
LR__0004
	shl	_var02, #1 wc
 if_nc	djnz	_var03, #LR__0004
	mov	_var01, #33
	sub	_var01, _var03
	shl	arg03, _var01
	mov	_var04, #3
	sub	_var04, _var01
	add	arg02, _var04
	add	arg03, #256
	mov	_var05, arg03
	andn	_var05, #255 wz
 if_e	add	arg02, #1
	add	arg02, #127
	mins	arg02, imm_4294967273_
	maxs	arg02, #255
	cmps	arg02, #1 wc,wz
 if_ae	jmp	#LR__0005
	shr	arg03, #1
	mov	_var05, imm_2147483648_
	add	_var05, arg03
	neg	arg02, arg02
	shr	_var05, arg02
	mov	arg03, _var05
	mov	arg02, #0
LR__0005
	shl	arg01, #31
	mov	result1, arg01
	shl	arg02, #23
	or	result1, arg02
	shr	arg03, #9
	or	result1, arg03
	jmp	#__system___float_Pack_ret
LR__0006
	mov	result1, _var01
__system___float_Pack_ret
	ret

unsmultiply_
	mov	itmp2_, #0
	jmp	#do_multiply_

multiply_
	mov	itmp2_, muldiva_
	xor	itmp2_, muldivb_
	abs	muldiva_, muldiva_
	abs	muldivb_, muldivb_
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
	ret

__lockreg
	long	0
imm_1199570944_
	long	1199570944
imm_2147483648_
	long	-2147483648
imm_4294967273_
	long	-23
imm_4294967295_
	long	-1
imm_536870912_
	long	536870912
imm_8388607_
	long	8388607
itmp1_
	long	0
itmp2_
	long	0
muldiva_
	long	0
muldivb_
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
result2
	long	0
result3
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
__system___float_mul_ma
	res	1
__system___float_mul_sa
	res	1
__system___float_mul_sb
	res	1
__system___float_mul_xa
	res	1
__system___float_mul_xb
	res	1
__system___float_tointeger__cse__0001
	res	1
__system___float_tointeger_m
	res	1
__system___float_tointeger_r
	res	1
__system___float_tointeger_x
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
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
