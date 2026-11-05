pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_insertstr
	add	arg01, arg03
	mov	result1, arg01
	mov	result2, arg02
_insertstr_ret
	ret

_main
	mov	arg01, #1
	mov	arg02, #2
	mov	arg03, #99
	call	#_insertstr
	mov	outa, #1
	mov	outb, result1
_main_ret
	ret

result1
	long	0
result2
	long	1
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
