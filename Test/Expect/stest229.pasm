pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_show
	mov	outa, arg02
	mov	outb, arg03
_show_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
_var03
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
