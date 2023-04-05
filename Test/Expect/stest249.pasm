pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_insertstr
	mov	result1, arg01
	mov	_var02, arg02
	add	result1, arg03
	mov	result2, _var02
_insertstr_ret
	ret

_main
	mov	_main_p_0007, #1
	mov	_main_p_0007_01, #2
	mov	arg02, _main_p_0007_01
	mov	arg01, _main_p_0007
	mov	arg03, #99
	call	#_insertstr
	mov	_main_q_0008, result1
	mov	_main_q_0008_01, result2
	mov	outa, _main_p_0007
	mov	outb, _main_q_0008
_main_ret
	ret

result1
	long	0
result2
	long	1
COG_BSS_START
	fit	496
	org	COG_BSS_START
_main_p_0007
	res	1
_main_p_0007_01
	res	1
_main_q_0008
	res	1
_main_q_0008_01
	res	1
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
