pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getval
	cmps	arg01, #2 wc,wz
 if_b	mov	result1, #99
 if_ae	mov	result1, #100
_getval_ret
	ret

_main
	mov	outa, #99
	mov	outb, #100
	mov	result1, #0
_main_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
