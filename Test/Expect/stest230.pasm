pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_show
	mov	outa, arg01
	mov	outb, arg02
	mov	ina, arg03
_show_ret
	ret

_main
	mov	outa, #1
	mov	outb, #2
	mov	ina, #3
	mov	result1, #0
_main_ret
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
