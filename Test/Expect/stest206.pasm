pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_clamp
	maxs	arg01, #127 wc
	mins	arg01, #0 wc
	mov	result1, arg01
_clamp_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
