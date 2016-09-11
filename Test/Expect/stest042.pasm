PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_select
	cmps	arg1, #0 wz
 if_ne	mov	_tmp001_, arg2
 if_e	mov	_tmp002_, arg3
 if_e	add	_tmp002_, #2
 if_e	mov	_tmp001_, _tmp002_
	mov	result1, _tmp001_
_select_ret
	ret

_tmp001_
	long	0
_tmp002_
	long	0
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
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
