pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_dummy
_dummy_ret
	ret
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
