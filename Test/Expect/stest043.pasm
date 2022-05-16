pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	cmps	arg01, #0 wc
 if_b	cmps	arg02, #0 wc
	subx	result1, result1
_blah_ret
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
