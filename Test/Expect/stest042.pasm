pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_select
	cmp	arg01, #0 wz
 if_ne	mov	result1, arg02
 if_e	add	arg03, #2
 if_e	mov	result1, arg03
_select_ret
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
arg03
	res	1
	fit	496
