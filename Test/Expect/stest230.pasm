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
	mov	_main_s__0001, #1
	mov	_main_s__0001_01, #2
	mov	_main_s__0001_02, #3
	mov	arg01, _main_s__0001
	mov	arg02, _main_s__0001_01
	mov	arg03, _main_s__0001_02
	call	#_show
	mov	result1, #0
_main_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_main_s__0001
	res	1
_main_s__0001_01
	res	1
_main_s__0001_02
	res	1
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
