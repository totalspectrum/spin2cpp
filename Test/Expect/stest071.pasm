DAT
	org	0

_blink
L_047_
	cmps	arg2, #0 wz
 if_e	jmp	#L_049_
	mov	blink_tmp002_, #1
	shl	blink_tmp002_, arg1
	xor	OUTA, blink_tmp002_
	sub	arg2, #1
	jmp	#L_047_
L_049_
	rdlong	result1, ptr__dat__
_blink_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
blink_tmp002_
	long	0
ptr__dat__
	long	@@@_dat_
result1
	long	0
	fit	496
	long
_dat_
	long	$11223344
	long	@@@_dat_ + 4
