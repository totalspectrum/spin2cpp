PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_select
	cmps	arg1, #0 wz
 if_ne	mov	select_tmp001_, arg2
 if_e	mov	select_tmp002_, arg3
 if_e	add	select_tmp002_, #2
 if_e	mov	select_tmp001_, select_tmp002_
	mov	result1, select_tmp001_
_select_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
select_tmp001_
	long	0
select_tmp002_
	long	0
	fit	496
