pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_gethibit
	sar	arg01, #31
	and	arg01, #1
	mov	result1, arg01
_gethibit_ret
	ret

_gethibits
	sar	arg01, #30
	and	arg01, #3
	mov	result1, arg01
_gethibits_ret
	ret

_getbit
	sar	arg01, arg02
	and	arg01, #1
	mov	result1, arg01
_getbit_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
