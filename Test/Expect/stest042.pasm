DAT
	org	0

_select
	cmps	arg1_, #0 wz
 if_ne	mov	select_tmp001_, arg2_
 if_e	mov	select_tmp002_, arg3_
 if_e	add	select_tmp002_, #2
 if_e	mov	select_tmp001_, select_tmp002_
	mov	result_, select_tmp001_
_select_ret
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
select_tmp001_
	long	0
select_tmp002_
	long	0
	fit	496
