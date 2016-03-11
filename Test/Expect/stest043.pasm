DAT
	org	0

_blah
	mov	blah_tmp001_, #0
	cmps	arg1_, #0 wc,wz
 if_ae	jmp	#L_032_
	cmps	arg2_, #0 wc,wz
 if_b	neg	blah_tmp001_, #1
L_032_
	mov	result_, blah_tmp001_
_blah_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
blah_tmp001_
	long	0
result_
	long	0
