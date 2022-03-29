pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_ismagic
	cmp	arg01, #511 wz
 if_e	mov	result1, #1
 if_ne	mov	result1, #0
_ismagic_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
