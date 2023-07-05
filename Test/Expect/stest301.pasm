pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_rcl_nc_const
	shl	arg01, #4
	mov	result1, arg01
_rcl_nc_const_ret
	ret

_rcl_c_const
	shl	arg01, #4
	or	arg01, #15
	mov	result1, arg01
_rcl_c_const_ret
	ret

_rcr_nc_const
	shr	arg01, #4
	mov	result1, arg01
_rcr_nc_const_ret
	ret

_rcr_c_const
	shr	arg01, #4
	or	arg01, imm_4026531840_
	mov	result1, arg01
_rcr_c_const_ret
	ret

_rcl_nc_dyn
	shl	arg01, arg02
	mov	result1, arg01
_rcl_nc_dyn_ret
	ret

_rcl_c_dyn
	mov	result1, #0
	cmp	result1, #1 wc
	rcl	arg01, arg02
	mov	result1, arg01
_rcl_c_dyn_ret
	ret

_rcl_c_fold
	mov	result1, #47
_rcl_c_fold_ret
	ret

_rcl_nc_fold
	mov	result1, #32
_rcl_nc_fold_ret
	ret

_rcl_c_wz
	mov	result1, #0
	cmp	result1, #1 wc
	rcl	arg01, #4 wz
	negz	result1, arg01
_rcl_c_wz_ret
	ret

_rcl_nc_wz
	shl	arg01, #4 wz
	negz	result1, arg01
_rcl_nc_wz_ret
	ret

_addx_nc_const
	add	arg01, #1
	mov	result1, arg01
_addx_nc_const_ret
	ret

_addx_c_const
	add	arg01, #2
	mov	result1, arg01
_addx_c_const_ret
	ret

_subx_nc_const
	sub	arg01, #1
	mov	result1, arg01
_subx_nc_const_ret
	ret

_subx_c_const
	sub	arg01, #2
	mov	result1, arg01
_subx_c_const_ret
	ret

_addx_c_const_noop
	mov	result1, arg01
_addx_c_const_noop_ret
	ret

_addx_c_const_noop2
	or	arg01, #123
	mov	result1, arg01
_addx_c_const_noop2_ret
	ret

_addx_nc_dyn
	add	arg01, arg02
	mov	result1, arg01
_addx_nc_dyn_ret
	ret

_addx_c_dyn
	mov	result1, #0
	cmp	result1, #1 wc
	addx	arg01, arg02
	mov	result1, arg01
_addx_c_dyn_ret
	ret

_addx_c_full_fold
	neg	result1, #16
	mov	result2, #8
	add	result1, #15 wc
	addx	result2, #1
_addx_c_full_fold_ret
	ret

_bad_instr
	mov	result1, #0
	cmp	result1, #1 wc
	addsx	arg01, arg02
	subsx	arg01, arg02
	rcl	arg01, arg02 wc
	mov	result1, arg01
_bad_instr_ret
	ret

imm_4026531840_
	long	-268435456
result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
