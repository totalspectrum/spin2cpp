pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	rdbyte	_var01, arg01
	wrbyte	_var01, arg01
	add	arg01, #1
	mov	outa, arg01
_main_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
