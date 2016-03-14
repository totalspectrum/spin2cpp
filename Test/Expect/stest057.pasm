DAT
	org	0

_myfill
L_036_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_036_
_myfill_ret
	ret

_fillzero
	mov	arg3_, arg2_
	mov	arg2_, #0
L_032_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_032_
_fillzero_ret
	ret

_fillone
	mov	arg3_, arg2_
	neg	arg2_, #1
L_035_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_035_
_fillone_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
result_
	long	0
	fit	496
