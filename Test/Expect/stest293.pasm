pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getval
	rdlong	result1, arg02
_getval_ret
	ret

_getval2
	mov	arg01, ptr__dat__
	add	ptr__dat__, #4
	mov	arg02, ptr__dat__
	sub	ptr__dat__, #4
	call	#_getval
_getval2_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00, $00, $00, $00, $00, $00, $00, $00
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
