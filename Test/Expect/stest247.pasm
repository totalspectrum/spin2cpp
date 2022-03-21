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
	mov	__system___float_tointeger_r, arg02
	mov	__system___float_Unpack_s, arg01
	shr	__system___float_Unpack_s, #31
	mov	__system___float_Unpack_x, arg01
	shl	__system___float_Unpack_x, #1
	shr	__system___float_Unpack_x, #24 wz
	and	arg01, imm_8388607_
 if_ne	shl	arg01, #6
 if_ne	or	arg01, imm_536870912_
 if_ne	jmp	#LR__0002
	mov	_tmp002_, arg01
	mov	_tmp001_, #32
LR__0001
	shl	_tmp002_, #1 wc
 if_ae	djnz	_tmp001_, #LR__0001
	sub	_tmp001_, #23
	mov	__system___float_Unpack_x, _tmp001_
	mov	__system___float_Unpack__cse__0006, #7
	sub	__system___float_Unpack__cse__0006, _tmp001_
	shl	arg01, __system___float_Unpack__cse__0006
LR__0002
	sub	__system___float_Unpack_x, #127
	mov	result3, arg01
	mov	result2, __system___float_Unpack_x
	mov	__system___float_tointeger_x, result2
	mov	__system___float_tointeger_m, result3
	cmps	__system___float_tointeger_x, imm_4294967295_ wc
 if_b	jmp	#LR__0003
	cmps	__system___float_tointeger_x, #31 wc
 if_ae	jmp	#LR__0003
	shl	__system___float_tointeger_m, #2
	mov	__system___float_tointeger__cse__0001, #30
	sub	__system___float_tointeger__cse__0001, __system___float_tointeger_x
	shr	__system___float_tointeger_m, __system___float_tointeger__cse__0001
	add	__system___float_tointeger_m, __system___float_tointeger_r
	shr	__system___float_tointeger_m, #1
	cmp	__system___float_Unpack_s, #0 wz
 if_ne	neg	__system___float_tointeger_m, __system___float_tointeger_m
	mov	result1, __system___float_tointeger_m
	jmp	#__system___float_tointeger_ret
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
	long	0
result3
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
__system___float_Unpack__cse__0006
	res	1
__system___float_Unpack_s
	res	1
__system___float_Unpack_x
	res	1
__system___float_tointeger__cse__0001
	res	1
__system___float_tointeger_m
	res	1
__system___float_tointeger_r
	res	1
__system___float_tointeger_x
	res	1
_tmp001_
	res	1
_tmp002_
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
