DAT
	org	0

_myfill
L_029_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_029_
_myfill_ret
	ret

_fillzero
	mov	arg3_, arg2_
	mov	arg2_, #0
L_025_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_025_
_fillzero_ret
	ret

_fillone
	mov	arg3_, arg2_
	neg	arg2_, #1
L_028_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_028_
_fillone_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
