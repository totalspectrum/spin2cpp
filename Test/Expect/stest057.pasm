DAT
	org	0

_myfill
L_020_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_020_
_myfill_ret
	ret

_fillzero
	mov	arg3_, arg2_
	mov	arg2_, #0
L_016_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_016_
_fillzero_ret
	ret

_fillone
	mov	arg3_, arg2_
	neg	arg2_, #1
L_019_
	wrlong	arg2_, arg1_
	add	arg1_, #4
	djnz	arg3_, #L_019_
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
