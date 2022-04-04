pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	outa, #99
	mov	outb, #100
_demo_ret
	ret

_getval
	cmp	arg01, #1 wz
 if_e	mov	result1, #99
 if_ne	mov	result1, #100
_getval_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
	fit	496
