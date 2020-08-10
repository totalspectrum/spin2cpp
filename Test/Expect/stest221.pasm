con
	_clkfreq = 160000000
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	arg01, #2
	call	#_barg
_demo_ret
	ret

_barg
	cmps	arg01, #1 wc,wz
 if_b	mov	_var01, #1
 if_ae	neg	_var01, #1
	mov	_var02, #1
	add	_var02, _var01
LR__0001
	mov	outb, arg01
	mov	outb, ptr_L__0006_
	add	arg01, _var01
	cmp	arg01, _var02 wz
 if_ne	jmp	#LR__0001
_barg_ret
	ret

ptr_L__0006_
	long	@@@LR__0002
COG_BSS_START
	fit	496

LR__0002
	byte	"goodbye"
	byte	0
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
	fit	496
