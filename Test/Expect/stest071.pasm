PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_blink
	mov	_var_05, #1
	shl	_var_05, arg1
	cmps	arg2, #0 wz
 if_e	jmp	#L__0006
L__0007
	xor	OUTA, _var_05
	djnz	arg2, #L__0007
L__0006
	rdlong	result1, ptr__dat__
_blink_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	long	$11223344
	long	@@@_dat_ + 4
	org	COG_BSS_START
_var_05
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
