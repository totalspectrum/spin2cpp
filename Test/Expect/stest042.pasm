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

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_tmp001_
	res	1
_tmp002_
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
