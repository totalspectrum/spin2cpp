pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mylockset
	lockset	arg01
_mylockset_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
