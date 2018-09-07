PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_lockset
	lockset	arg1
_lockset_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
arg1
	res	1
	fit	496
