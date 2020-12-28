con
	_clkfreq = 160000000
	mychoice = 3
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_blah_i, #1
	mov	arg01, #4
LR__0001
	mov	outb, _blah_i
	mov	outb, ptr_L__0011_
	add	_blah_i, #1
	cmp	_blah_i, arg01 wz
 if_ne	jmp	#LR__0001
_demo_ret
	ret

__lockreg
	long	0
ptr_L__0011_
	long	@@@LR__0002
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496

LR__0002
	byte	"hello"
	byte	0
	org	COG_BSS_START
_blah_i
	res	1
arg01
	res	1
	fit	496
