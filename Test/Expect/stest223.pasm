con
	_clkfreq = 160000000
	mychoice = 3
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_var01, #1
	mov	arg01, #4
LR__0001
	mov	outb, _var01
	mov	outb, ptr_L__0011_
	add	_var01, #1
	cmp	_var01, arg01 wz
 if_ne	jmp	#LR__0001
_demo_ret
	ret

ptr_L__0011_
	long	@@@LR__0002
COG_BSS_START
	fit	496

LR__0002
	byte	"hello"
	byte	0
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
