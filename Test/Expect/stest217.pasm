pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getval
	cmp	arg01, #2 wc
 if_b	mov	result1, #99
 if_ae	mov	result1, #100
_getval_ret
	ret

_main
	mov	outa, #100
	mov	outb, #100
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
	fit	496
