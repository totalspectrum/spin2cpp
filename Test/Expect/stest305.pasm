pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	rdword	_var01, arg01
	and	_var01, #7
	wrword	_var01, arg01
_blah_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
