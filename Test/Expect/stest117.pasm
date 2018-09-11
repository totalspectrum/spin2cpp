PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_uscmp
	mov	_var_00, #0
	cmp	arg1, #2 wc,wz
 if_e	mov	_var_00, #1
	mov	result1, _var_00
_uscmp_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_00
	res	1
arg1
	res	1
	fit	496
